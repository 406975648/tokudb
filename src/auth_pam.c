/*
(C) 2011 Percona Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/**
 @file

 PAM authentication for MySQL, server- and client-side plugins for the
 production use.

 A general-purpose PAM authentication plugin for MySQL.  Acts as a mediator
 between the MySQL server, the MySQL client, and the PAM backend.  Consists of
 both the server and the client plugin.

 The server plugin requests authentication from the PAM backend, forwards any
 requests and messages from the PAM backend over the wire to the client (in
 cleartext) and reads back any replies for the backend.

 The client plugin inputs from the TTY any requested info/passwords as
 requested and sends them back over the wire to the server.

 This plugin does not encrypt the communication channel in any way.  If this is
 required, a SSL connection should be used.

 To install this plugin, copy the .so file to the plugin directory and do

 INSTALL PLUGIN auth_pam_server SONAME 'auth_pam.so';

 To use this plugin for one particular user, specify it at user's creation time
 (TODO: tested with localhost only):

 CREATE USER 'username'@'hostname' IDENTIFIED WITH auth_pam_server;

 Alternatively UPDATE the mysql.user table to set the plugin value for an
 existing user.

 Also it is possible to use this plugin to authenticate anonymous users:

 CREATE USER ''@'hostname' IDENTIFIED WITH auth_pam_server;

*/

#include <assert.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_misc.h>

/* Define these macros ourselves, so we don't have to include my_global.h and
can compile against unconfigured MySQL source tree.  */
#define STDCALL

#define MY_ASSERT_UNREACHABLE() assert(0)

#include <mysql/plugin.h>
#include <mysql/plugin_auth.h>
#include <mysql/client_plugin.h>

#include <unistd.h> /* getpass() */

#include "lib_auth_pam_client.h"

/* The server plugin */

/** The MySQL service name for PAM configuration */
static const char* service_name= "mysqld";

static int valid_pam_msg_style (int pam_msg_style)
{
  switch (pam_msg_style)
  {
  case PAM_PROMPT_ECHO_OFF:
  case PAM_PROMPT_ECHO_ON:
  case PAM_ERROR_MSG:
  case PAM_TEXT_INFO:
    return 1;
  default:
    return 0;
  }
}

static char pam_msg_style_to_char (int pam_msg_style)
{
  /* Do not use any of MySQL client-server protocol reserved values, i.e. \0 */
  switch (pam_msg_style)
  {
  case PAM_PROMPT_ECHO_OFF: return '\2';
  case PAM_PROMPT_ECHO_ON:  return '\3';
  case PAM_ERROR_MSG:       return '\4';
  case PAM_TEXT_INFO:       return '\5';
  default:
    MY_ASSERT_UNREACHABLE();
    return '\1';
  }
}

static void free_pam_response (struct pam_response ** resp, int n)
{
  int i;
  for (i = 0; i < n; i++)
  {
    free((*resp)[i].resp);
  }
  free(*resp);
  *resp= NULL;
}

static int vio_server_conv (int num_msg, const struct pam_message **msg,
                            struct pam_response ** resp, void *appdata_ptr)
{
  int i;
  int pkt_len;
  MYSQL_PLUGIN_VIO *vio= NULL;

  if (appdata_ptr == NULL)
  {
    MY_ASSERT_UNREACHABLE();
    return PAM_CONV_ERR;
  }
  vio= (MYSQL_PLUGIN_VIO *)appdata_ptr;

  *resp = calloc (sizeof (struct pam_response), num_msg);
  if (*resp == NULL)
    return PAM_BUF_ERR;

  for (i = 0; i < num_msg; i++)
  {
    char *buf;

    if (!valid_pam_msg_style(msg[i]->msg_style))
    {
      free_pam_response(resp, i);
      return PAM_CONV_ERR;
    }

    /* Format the message.  The first byte is the message type, followed by the
    NUL-terminated message string itself.  */
    buf= malloc(strlen(msg[i]->msg) + 2);
    if (!buf)
    {
      free_pam_response(resp, i);
      return PAM_BUF_ERR;
    }
    buf[0]= pam_msg_style_to_char(msg[i]->msg_style);
    strcpy(buf + 1, msg[i]->msg);

    /* Write the message.  */
    if (vio->write_packet(vio, (unsigned char *)buf, strlen(buf) + 1))
    {
      free(buf);
      free_pam_response(resp, i);
      return PAM_CONV_ERR;
    }
    free(buf);

    /* Is the answer expected? */
    if ((msg[i]->msg_style != PAM_PROMPT_ECHO_ON)
        && (msg[i]->msg_style != PAM_PROMPT_ECHO_OFF))
      continue;

    /* Read the answer */
    if ((pkt_len= vio->read_packet(vio, (unsigned char **)&buf)) < 0)
    {
      free_pam_response(resp, i);
      return PAM_CONV_ERR;
    }

    (*resp)[i].resp= malloc(pkt_len + 1);
    if ((*resp)[i].resp == NULL)
    {
      free_pam_response(resp, i);
      return PAM_BUF_ERR;
    }
    strncpy((*resp)[i].resp, buf, pkt_len);
    (*resp)[i].resp[pkt_len]= '\0';
  }
  return PAM_SUCCESS;
}

static int authenticate_user_with_pam_server (MYSQL_PLUGIN_VIO *vio,
                                              MYSQL_SERVER_AUTH_INFO *info)
{
  pam_handle_t *pam_handle;
  struct pam_conv conv_func_info= { &vio_server_conv, vio };
  int error;
  char *pam_mapped_user_name;

  /* Impossible to tell if PAM will use passwords or something else */
  info->password_used= PASSWORD_USED_NO_MENTION;

  error= pam_start(service_name,
                   info->user_name,
                   &conv_func_info, &pam_handle);
  if (error != PAM_SUCCESS)
    return CR_ERROR;

  error= pam_set_item(pam_handle, PAM_RUSER, info->user_name);
  if (error != PAM_SUCCESS)
  {
    pam_end(pam_handle, error);
    return CR_ERROR;
  }

  error= pam_set_item(pam_handle, PAM_RHOST, info->host_or_ip);
  if (error != PAM_SUCCESS)
  {
    pam_end(pam_handle, error);
    return CR_ERROR;
  }

  error= pam_authenticate(pam_handle, 0);
  if (error != PAM_SUCCESS)
  {
    pam_end(pam_handle, error);
    return CR_ERROR;
  }

  error= pam_acct_mgmt(pam_handle, 0);
  if (error != PAM_SUCCESS)
  {
    pam_end(pam_handle, error);
    return CR_ERROR;
  }

  /* Send end-of-auth message */
  if (vio->write_packet(vio, (const unsigned char *)"\0", 1))
  {
    pam_end(pam_handle, error);
    return CR_ERROR;
  }

  /* Get the authenticated user name from PAM */
  error= pam_get_item(pam_handle, PAM_USER, (void *)&pam_mapped_user_name);
  if (error != PAM_SUCCESS)
  {
    pam_end(pam_handle, error);
    return CR_ERROR;
  }

  /* Check if user name from PAM is the same as provided for MySQL.  If
  different, use the new user name for MySQL authorization and as
  CURRENT_USER() value.  */
  if (strcmp(info->user_name, pam_mapped_user_name))
  {
    strncpy(info->authenticated_as, pam_mapped_user_name,
            MYSQL_USERNAME_LENGTH);
    info->authenticated_as[MYSQL_USERNAME_LENGTH]= '\0';
  }

  error= pam_end(pam_handle, error);
  if (error != PAM_SUCCESS)
    return CR_ERROR;

  return CR_OK;
}

static struct st_mysql_auth pam_auth_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "auth_pam",
  &authenticate_user_with_pam_server
};

static struct st_mysql_auth test_pam_auth_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "auth_pam_test",
  &authenticate_user_with_pam_server
};

mysql_declare_plugin(auth_pam)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &pam_auth_handler,
  "auth_pam_server",
  "Percona, Inc.",
  "PAM authentication plugin",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0001,
  NULL,
  NULL,
  NULL
#if MYSQL_PLUGIN_INTERFACE_VERSION >= 0x103
  ,
  0
#endif
},
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &test_pam_auth_handler,
  "test_auth_pam_server",
  "Percona, Inc.",
  "PAM authentication plugin that requests the test version of the"
  " client-side plugin. DO NOT USE IN PRODUCTION.",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0001,
  NULL,
  NULL,
  NULL
#if MYSQL_PLUGIN_INTERFACE_VERSION >= 0x103
  ,
  0
#endif
}
mysql_declare_plugin_end;

/* The client plugin */

/* Returns malloc-allocated string, NULL in case of memory error. */
static char * prompt_echo_off (const char * prompt)
{
  /* TODO: getpass not thread safe.  Probably not a big deal in the mysql
     client program, but may be missing on non-glibc systems.  */
  char* getpass_input= getpass(prompt);
  return strdup(getpass_input);
}

/* Returns malloc-allocated string, NULL in case of memory or (unlikely)
I/O error.  */
static char * prompt_echo_on (const char * prompt)
{
  char* c;
  char fgets_buf[1024];
  fputs(prompt, stdout);
  fputc(' ', stdout);
  c= fgets(fgets_buf, sizeof(fgets_buf), stdin);
  if (c == NULL)
  {
    if (ferror(stdin))
      return NULL;
    fgets_buf[0]= '\0';
  }
  if ((c= strchr(fgets_buf, '\n')))
    *c= '\0';
  else
    fgets_buf[sizeof(fgets_buf) - 1]= '\0';
  return strdup(fgets_buf);
}

static void show_error(const char * message)
{
  fprintf(stderr, "ERROR %s\n", message);
}

static void show_info(const char * message)
{
  printf("%s\n", message);
}

static int authenticate_user_with_pam_client (MYSQL_PLUGIN_VIO *vio,
                                              struct st_mysql *mysql)
{
  return authenticate_user_with_pam_client_common (vio, mysql,
                                                   &prompt_echo_off,
                                                   &prompt_echo_on,
                                                   &show_error, &show_info);
}

mysql_declare_client_plugin(AUTHENTICATION)
  "auth_pam",
  "Percona, Inc.",
  "PAM authentication plugin",
  {0,1,0},
  "GPL",
  NULL,
  NULL, /* init */
  NULL, /* deinit */
  NULL, /* options */
  &authenticate_user_with_pam_client
mysql_end_client_plugin;

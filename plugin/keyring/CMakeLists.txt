# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

INCLUDE_DIRECTORIES(SYSTEM ${BOOST_PATCHES_DIR} ${BOOST_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/common)

SET(
    KEYRING_FILE_SOURCES
    common/keyring_key.cc
    common/keys_container.cc
    common/keys_iterator.cc
    common/system_keys_container.cc
    common/system_key_adapter.cc
    common/keyring_impl.cc
    keyring.cc
    hash_to_buffer_serializer.cc
    buffered_file_io.cc
    file_io.cc
    buffer.cc
)

MYSQL_ADD_PLUGIN(keyring_file
                 ${KEYRING_FILE_SOURCES}
                 MODULE_ONLY
                 MODULE_OUTPUT_NAME "keyring_file")

IF(SSL_DEFINES)
  ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

IF(HAVE_DLOPEN)
    SET(DYNLIB_EXTENSION "so")
    SET_TARGET_PROPERTIES(keyring_file
      PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
ENDIF()

# We limit symbols exported on Linux to only those required by server.
IF(LINK_FLAG_NO_UNDEFINED)
  GET_PROPERTY(keyring_file_link_flags TARGET keyring_file PROPERTY LINK_FLAGS)
  SET_PROPERTY(TARGET keyring_file PROPERTY LINK_FLAGS "${keyring_file_link_flags} -Wl,--version-script=${CMAKE_SOURCE_DIR}/plugin/keyring/keyring_file.version")
ENDIF()

# Boost source has unused local typedefs.
MY_CHECK_CXX_COMPILER_FLAG("-Wunused-local-typedefs" HAVE_UNUSED_TYPEDEFS)
IF(HAVE_UNUSED_TYPEDEFS)
  ADD_COMPILE_FLAGS(${KEYRING_FILE_SOURCES}
    COMPILE_FLAGS "-Wno-unused-local-typedefs")
ENDIF()

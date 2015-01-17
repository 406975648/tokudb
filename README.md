The Tokutek hot backup library intercepts system calls that write files and duplicates the writes on backup files. It does this while copying files to the backup directory.  There are two technical issues to address to get hot backup working with MySQL.

First, the hot backup library must be loaded into the mysqld server so that it can intercept system calls.  We use LD_PRELOAD to solve the system call intercept problem.  Alternatively, the hot backup library can be linked into mysqld.

Second, there must be a user interface that can be used to start a backup, track its progress, and determine whether or not the backup succeeded.  We use a plugin that kicks off a backup as a side effect of setting a backup session variable to the name of the destination directory.

# Install the hot backup libraries
1 Extract the tarball

2 Copy lib/mysql/plugin/tokudb_backup.so to MySQL's lib/mysql/plugin directory

3 Copy lib/libHotBackup.so to MySQL's lib directory

4 Init mysqld
```
scripts/mysql_install_db
```

5 Run mysqld with the hot backup library (should exist in the lib directory)
```
LD_PRELOAD=PATH_TO_MYSQL_BASE_DIR/lib/libHotBackup.so ./mysqld_safe
```

6 Install the backup plugin (should exist in the lib/mysql/plugin directory)
```
install plugin tokudb_backup soname 'tokudb_backup.so';
````

# Run a backup

1 Backup to the '/tmp/backup1047' directory.  This blocks until the backup is complete.
```
set tokudb_backup_dir='/tmp/backup1047';
```

2 Check if the backup worked
```
select @@tokudb_backup_last_error, @@tokudb_backup_last_error_string;
```

# Monitor progress
The Tokutek hot backup updates the processlist state with progress information while it is running.

# Throttle the backup write rate
The ```tokudb_backup_throttle``` variable imposes an upper bound on the write rate of the TokuDB backup.  Units are bytes per second.  Default is no upper bound.

# Build the hot backup plugin from source
1 Checkout the Percona Server source

2 Checkout the tokudb backup plugin with tag 'tokudb-backup-0.14'
```
cd percona-server-5.6/plugin
git clone git@github.com:Tokutek/tokudb-backup-plugin
pushd tokudb-backup-plugin
git checkout tokudb-backup-0.14
popd
```

3 Checkout the tokudb hot backup library with tag 'tokudb-backup-0.14'
```
cd percona-server-5.6/plugin/tokudb-backup-plugin
git clone git@github.com:Tokutek/backup-enterprise
ln -s backup-enterprise/backup backup
pushd backup-enterprise
git checkout tokudb-backup-0.14
popd
```

4 Build
```
cmake -DBUILD_CONFIG=mysql_release -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=../tokudb-backup-plugin-0.14-percona-server-5.6.21 -DTOKUDB_BACKUP_PLUGIN_VERSION=tokudb-backup-0.14 ../percona-server-5.6.21
cd plugin/tokudb-backup-plugin
make -j8 install
```

5 Make tarball
```
tar czf tokudb-backup-plugin-0.14-percona-server-5.6.21.tar.gz tokudb-backup-plugin-0.14-percona-server-5.6.21
```

# Variables
## tokudb_backup_plugin_version
* name:tokudb_backup_plugin_version
* readonly:true
* scope:system
* type:str
* comment:version of the tokudb backup plugin

## tokudb_backup_version
* name:tokudb_backup_version
* readonly:true
* scope:system
* type:str
* comment:version of the tokutek backup library

## tokudb_backup_allowed_prefix
* name:tokudb_backup_allowed_prefix
* readonly:true
* scope:system
* type:str
* comment:destination directory prefix

## tokudb_backup_throttle
* name:tokudb_backup_throttle
* readonly:false
* scope:session
* type:ulonglong
* def_val:18446744073709551615
* min_val:0
* max_val:18446744073709551615
* comment:backup throttle on write rate in bytes per second.  default is unlimited

## tokudb_backup_dir
* name:tokudb_backup_dir
* readonly:false
* scope:session
* type:str
* comment:name of the directory where the backup is stored

## tokudb_backup_last_error
* name:tokudb_backup_last_error
* readonly:false
* scope:session
* type:ulong
* def_val:0
* min_val:0
* max_val:18446744073709551615
* comment:last error

## tokudb_backup_last_error_string
* name:tokudb_backup_last_error_string
* readonly:false
* scope:session
* type:str
* comment:last error string


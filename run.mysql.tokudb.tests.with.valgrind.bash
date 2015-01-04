for suite in funcs iuds ; do
    ./mtr --suite=engines/$suite --mysqld=--default-storage-engine=tokudb --mysqld=--default-tmp-storage-engine=tokudb --mysqld=--plugin-load=tokudb=ha_tokudb.so --mysqld=--loose-tokudb-check-jemalloc=0 --parallel=auto --force --retry=0 --max-test-fail=0 --valgrind-mysqld --valgrind-option=--suppressions=tokudb.supp --valgrind-option=--show-reachable=yes >$suite.valgrind.out 2>&1
done

for suite in tokudb tokudb.bugs tokudb.add_index tokudb.alter_table ; do
    ./mtr --suite=$suite --skip-test='fast_up.*\|rows-32m.*\|5585\|xa-.*' --mysqld='--plugin-load=tokudb=ha_tokudb.so;tokudb_trx=ha_tokudb.so;tokudb_locks=ha_tokudb.so;tokudb_lock_waits=ha_tokudb.so;tokudb_fractal_tree_info=ha_tokudb.so' --mysqld=--loose-tokudb-check-jemalloc=0 --force --retry=0 --max-test-fail=0 --parallel=auto --no-warnings --testcase-timeout=60 --valgrind-mysqld --valgrind-option=--suppressions=tokudb.supp --valgrind-option=--show-reachable=yes >$suite.valgrind.out 2>&1 
done

./mtr  --suite=parts --do-test=.*tokudb.* --mysqld=--plugin-load=tokudb=ha_tokudb.so --mysqld=--loose-tokudb-check-jemalloc=0 --force --retry=0 --max-test-fail=0 --parallel=auto --no-warnings --testcase-timeout=60 --valgrind-mysqld --valgrind-option=--suppressions=tokudb.supp --valgrind-option=--show-reachable=yes >parts.valgrind.out 2>&1

./mtr  --suite=rpl --do-test=.*tokudb.* --mysqld=--plugin-load=tokudb=ha_tokudb.so --mysqld=--loose-tokudb-check-jemalloc=0 --force --retry=0 --max-test-fail=0 --parallel=auto --no-warnings --testcase-timeout=60 --valgrind-mysqld --valgrind-option=--suppressions=tokudb.supp --valgrind-option=--show-reachable=yes >rpl.valgrind.out 2>&1

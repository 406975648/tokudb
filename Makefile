FETCH_CMD=wget
MASTER_SITE=http://www.percona.com/downloads/community
MYSQL_VERSION=5.1.57
PERCONA_SERVER_VERSION=rel12.8
PERCONA_SERVER         ?=Percona-Server-$(MYSQL_VERSION)-$(PERCONA_SERVER_VERSION)
PERCONA_SERVER_SHORT_1 ?=Percona-Server-$(MYSQL_VERSION)
PERCONA_SERVER_SHORT_2 ?=Percona-Server

all: main install-lic tests misc handlersocket maatkit-udf autorun
	@echo ""
	@echo "Percona Server source code is ready"
	@echo "Now change directory to $(PERCONA_SERVER) define variables as show below"
	@echo ""
	export CFLAGS="-O2 -g -fmessage-length=0 -D_FORTIFY_SOURCE=2"
	export CXXFLAGS="-O2 -g -fmessage-length=0 -D_FORTIFY_SOURCE=2"
	export LIBS=-lrt
	@echo ""
	@echo "and run ./configure --with-plugins=partition,archive,blackhole,csv,example,federated,innodb_plugin --without-embedded-server --with-pic --with-extra-charsets=complex --with-ssl --enable-assembler --enable-local-infile --enable-thread-safe-client --enable-profiling --with-readline && make all install"
	@echo ""

autorun:
	cd $(PERCONA_SERVER) && ./BUILD/autorun.sh

handlersocket:
	cp -R HandlerSocket-Plugin-for-MySQL $(PERCONA_SERVER)/storage

maatkit-udf:
	cp -R UDF "$(PERCONA_SERVER)"
	cd "$(PERCONA_SERVER)"/UDF && autoreconf --install

install-lic: 
	@echo "Installing license files"
	install -m 644 COPYING.* $(PERCONA_SERVER)

main: mysql-$(MYSQL_VERSION).tar.gz
	@echo "Prepare Percona Server sources"
	rm -rf mysql-$(MYSQL_VERSION)
	rm -rf $(PERCONA_SERVER)
	rm -rf $(PERCONA_SERVER_SHORT_1)
	rm -rf $(PERCONA_SERVER_SHORT_2)
	tar zxf mysql-$(MYSQL_VERSION).tar.gz
	mv mysql-$(MYSQL_VERSION) $(PERCONA_SERVER)
	ln -s $(PERCONA_SERVER) $(PERCONA_SERVER_SHORT_1)
	ln -s $(PERCONA_SERVER) $(PERCONA_SERVER_SHORT_2)
	ln -s ../patches $(PERCONA_SERVER)/patches
	(cd $(PERCONA_SERVER) && ../apply_patches)
 

mysql-$(MYSQL_VERSION).tar.gz:
	@echo "Downloading MySQL sources from $(MASTER_SITE)"
	$(FETCH_CMD) $(MASTER_SITE)/mysql-$(MYSQL_VERSION).tar.gz

tests:
	./install_tests.sh

misc:
	@echo "Installing other files"
	install -m 644 lrusort.py $(PERCONA_SERVER)/scripts

clean:
	rm -rf mysql-$(MYSQL_VERSION) $(PERCONA_SERVER)

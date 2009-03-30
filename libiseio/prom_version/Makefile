
prefix=/usr

libdir=$(prefix)/lib
bindir=$(prefix)/bin
includedir=$(prefix)/include

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644

CINCL = -I$(includedir) -L$(libdir)

#
# This program requires that the libiseio library and header file be
# installed ahead of time.
#
prom_version: prom_version.c
	$(CC) $(CFLAGS) $(CINCL) -o prom_version prom_version.c -liseio

install: $(bindir)/prom_version

$(bindir)/prom_version: prom_version
	$(INSTALL_PROGRAM) ./prom_version $(bindir)/prom_version


prefix = /usr
libdir = $(prefix)/lib
bindir = $(prefix)/bin
includedir = $(prefix)/include
sharedir = $(prefix)/share/ise

srcdir = .

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644

CFLAGS = -O -I$(includedir) -L$(libdir)

all: ise_getenv ise_setenv

ise_getenv: ise_getenv.c
	$(CC) -o ise_getenv $(CFLAGS) ise_getenv.c -liseio

ise_setenv: ise_setenv.c
	$(CC) -o ise_setenv $(CFLAGS) ise_setenv.c -liseio

environ-i960_ise.scof:
	@echo "ERROR: Need $@ from Picture Elements" && false

environ-i960_sse.scof:
	@echo "ERROR: Need $@ from Picture Elements" && false

environ-jse.scof:
	@echo "ERROR: Need $@ from Picture Elements" && false

install: all installdirs \
  $(bindir)/ise_getenv \
  $(bindir)/ise_setenv \
  $(sharedir)/environ-i960_ise.scof \
  $(sharedir)/environ-i960_sse.scof \
  $(sharedir)/environ-jse.scof

installdirs: mkinstalldirs
	$(srcdir)/mkinstalldirs $(bindir) $(sharedir)

$(bindir)/ise_getenv: ise_getenv
	$(INSTALL_PROGRAM) ise_getenv $(bindir)/ise_getenv

$(bindir)/ise_setenv: ise_setenv
	$(INSTALL_PROGRAM) ise_setenv $(bindir)/ise_setenv

$(sharedir)/environ-i960_ise.scof: environ-i960_ise.scof
	$(INSTALL_DATA) environ-i960_ise.scof $(sharedir)/environ-i960_ise.scof

$(sharedir)/environ-i960_sse.scof: environ-i960_sse.scof
	$(INSTALL_DATA) environ-i960_sse.scof $(sharedir)/environ-i960_sse.scof

$(sharedir)/environ-jse.scof: environ-jse.scof
	$(INSTALL_DATA) environ-jse.scof $(sharedir)/environ-jse.scof

uninstall:
	rm -f $(bindir)/ise_getenv
	rm -f $(bindir)/ise_setenv
	rm -f $(sharedir)/environ-i960_ise.scof
	rm -f $(sharedir)/environ-i960_sse.scof
	rm -f $(sharedir)/environ-jse.scof

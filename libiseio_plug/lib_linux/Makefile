
srcdir = .
prefix = /usr/local
exec_prefix = $(prefix)
libdir = $(exec_prefix)/lib
includedir = $(prefix)/include

CFLAGS = -O -g

all: libiseio_plug.a

L = lib_main.o channel.o command.o frame.o

%.o: %.c ../libiseio_plug.h priv.h
	$(CC) -c -o $@ -I.. $(CFLAGS) $*.c

libiseio_plug.a: $L
	rm -f libiseio_plug.a
	ar cqv libiseio_plug.a $L

lib_main.o: lib_main.c ../libiseio_plug.h priv.h
channel.o:  channel.c  ../libiseio_plug.h priv.h
command.o:  command.c  ../libiseio_plug.h priv.h
frame.o:    frame.c    ../libiseio_plug.h priv.h

install: all installdirs $(libdir)/libiseio_plug.a $(includedir)/libiseio.h

installdirs: mkinstalldirs
	$(srcdir)/mkinstalldirs $(libdir) $(includedir)

$(libdir)/libiseio_plug.a: libiseio_plug.a
	$(INSTALL_DATA) libiseio_plug.a $(libdir)/libiseio_plug.a

$(includedir)/libiseio_plug.h: $(srcdir)/../libiseio_plug.h
	$(INSTALL_DATA) $(srcdir)/../libiseio_plug.h $(includedir)/libiseio_plug.h

uninstall:
	rm -f $(includedir)/libiseio_plug.h
	rm -f $(libdir)/libiseio_plug.a


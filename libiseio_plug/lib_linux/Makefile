

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

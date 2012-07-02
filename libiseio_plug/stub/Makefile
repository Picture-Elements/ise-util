
all: stub.plg

stub.plg: stub.o ../lib_linux/libiseio_plug.a
	$(CC) -o stub.plg stub.o ../lib_linux/libiseio_plug.a -lpthread -lrt

stub.o: stub.c ../libiseio_plug.h
	$(CC) -c -o $@ -I.. $(CFLAGS) $*.c

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread
OBJS = echoclient.o echoserveri.o

all: echoclient echoserveri

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

echoclient.o: echoclient.c
	$(CC) $(CFLAGS) -c echoclient.c

echoserveri.o: echoserveri.c echo.c
	$(CC) $(CFLAGS) -c echoserveri.c echo.c	

echoclient: csapp.o echoclient.o
	$(CC) $(CFLAGS) csapp.o echoclient.o -o echoclient $(LDFLAGS)

echoserveri: csapp.o echoserveri.o echo.o
	$(CC) $(CFLAGS) csapp.o echoserveri.o echo.o -o echoserveri $(LDFLAGS)	

clean:
	rm -f echoclient echoserveri *.o
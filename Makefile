CC = gcc
CFLAGS = -Wall -Wextra -O2

all: clienteFTP

clienteFTP: CoronadoL-clienteFTP.o
	$(CC) $(CFLAGS) -o clienteFTP CoronadoL-clienteFTP.o

CoronadoL-clienteFTP.o: CoronadoL-clienteFTP.c
	$(CC) $(CFLAGS) -c CoronadoL-clienteFTP.c

clean:
	rm -f *.o clienteFTP

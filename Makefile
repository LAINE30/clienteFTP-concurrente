CC = gcc
CFLAGS = -Wall -Wextra -O2
OBJS = ZunigaJ-clienteFTP.o connectsock.o errexit.o

all: clienteftp

clienteftp: $(OBJS)
	$(CC) $(CFLAGS) -o clienteftp $(OBJS)

ZunigaJ-clienteFTP.o: ZunigaJ-clienteFTP.c connectsock.h errexit.h
	$(CC) $(CFLAGS) -c ZunigaJ-clienteFTP.c

connectsock.o: connectsock.c connectsock.h
	$(CC) $(CFLAGS) -c connectsock.c

errexit.o: errexit.c errexit.h
	$(CC) $(CFLAGS) -c errexit.c

clean:
	rm -f *.o clienteftp

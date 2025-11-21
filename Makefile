CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = CoronadoL-clienteFTP
SRC = CoronadoL-clienteFTP.c
OBJ = CoronadoL-clienteFTP.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

clean:
	rm -f *.o $(TARGET)

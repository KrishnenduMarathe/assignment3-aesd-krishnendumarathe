CC=$(CROSS_COMPILE)gcc
CFLAGS=-g -Wall -Werror

SOURCES=aesdsocket.c
OBJECTS=aesdsocket.o

all: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o aesdsocket

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm aesdsocket
	rm *.o

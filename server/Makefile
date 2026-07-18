CC=$(CROSS_COMPILE)gcc
CFLAGS=-g -Wall -Werror

SOURCES=aesdsocket.c
OBJECTS=aesdsocket.o
EXEC=aesdsocket

all: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	if [ -f $(EXEC) ]; then rm $(EXEC); fi
	if [ -f $(OBJECTS) ]; then rm $(OBJECTS); fi

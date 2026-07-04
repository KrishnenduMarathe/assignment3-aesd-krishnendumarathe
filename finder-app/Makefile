CC=$(CROSS_COMPILE)gcc
CFLAGS=-g

SOURCES=writer.c
OBJECTS=writer.o

all: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o writer

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm writer
	rm *.o

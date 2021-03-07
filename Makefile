SRC=$(wildcard *.c)

LIBS=-lpthread

all: $(SRC)
	gcc -g -o ping_parallel $^ $(CFLAGS) $(LIBS)
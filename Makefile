SRC=$(wildcard *.c)

LIBS=-lpthread

all: $(SRC)
	gcc -g -o ping $^ $(CFLAGS) $(LIBS)
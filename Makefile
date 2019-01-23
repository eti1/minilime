CC=g++
LD=g++
CFLAGS=-std=gnu++11 -Wall -Werror
LDFLAGS= $(shell  pkg-config --libs LimeSuite)
INC=$(shell  pkg-config --cflags LimeSuite)

all: lime_sample

%.o: %.cpp
	$(CC) $(INC) $(CFLAGS) -c -o $@ $^

lime_sample: src/main.o src/dev.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm lime_sample src/*.o

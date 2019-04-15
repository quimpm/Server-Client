#!bin/bash

LDFLAGS= -pedantic -Wall -ansi

all: client

client: Client.c
	gcc -o Client Client.c $(LDFLAGS)

clean:
	rm -f Client

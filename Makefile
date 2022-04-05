.PHONY: client server

all: client server

client:
	gcc -o client client.c -lpthread

server:
	gcc -o server server.c -lpthread
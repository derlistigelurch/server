all: server

server:
	gcc -Wall -o server server.c

clean:
	rm -f server

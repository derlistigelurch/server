all: server client

server:
	gcc -Wall -o server server.c

client:
	gcc -Wall -o client client.c

clean:
	rm -f server client

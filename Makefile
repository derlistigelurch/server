all: clean server client 

server:
	gcc -Wall -o server server.c myldap.c -lldap -llber -pthread

client:
	gcc -Wall -o client client.c 

clean:
	rm -f server client

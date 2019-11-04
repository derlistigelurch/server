all: clean server client 

server:
	gcc -Wall -o server server.c server_functions.c server_functions.h myldap.c -lldap -llber -pthread

client:
	gcc -Wall -o client client.c client_functions.c client_functions.h

clean:
	rm -f server client

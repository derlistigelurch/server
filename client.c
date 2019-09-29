/* myclient.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define BUF 1024
#define PORT 6543

int send_okay(int createsocket, char stringy[BUF], int length, int errorcode){
    send(createsocket, stringy, length, 0);
    recv(createsocket, stringy, BUF-1, 0);
    if (strncmp (stringy, "OK\n", 3) == 0){
        return 0;
	}
	else {
		switch (errorcode) {
		case 2: printf("ERROR! The sender must not have more than 8 characters\n"); break;
		case 3: printf("ERROR! The receiver must not have more than 8 characters\n"); break;
		case 4: printf("ERROR! The subject must not have more than 80 characters\n"); break;
		case 5: printf("ERROR! There was a problem with your message\n"); break;
		default: printf("An error has occurred!\n");
		}
		return errorcode;
	}
   /* }else
    return -1;*/
}


int main (int argc, char **argv) {
  int create_socket;
  char buffer[BUF];
  struct sockaddr_in address;
  int size;
  int errorc = 0;
  char sender[BUF];
  char receiver[BUF];
  char subject[BUF];
  char answer[BUF];

  if( argc < 2 ){
     printf("Usage: %s ServerAdresse\n", argv[0]);
     exit(EXIT_FAILURE);
  }

  if ((create_socket = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  {
     perror("Socket error");
     return EXIT_FAILURE;
  }
  
  memset(&address,0,sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons (PORT);
  inet_aton (argv[1], &address.sin_addr);

  if (connect ( create_socket, (struct sockaddr *) &address, sizeof (address)) == 0)
  {
	 system("clear");
     printf ("Connection with server (%s) established\n", inet_ntoa (address.sin_addr));
     size=recv(create_socket,buffer,BUF-1, 0);
     if (size>0)
     {
        buffer[size]= '\0';
        printf("%s",buffer);
	 }
  }
  else
  {
     perror("Connect error - no server available");
     return EXIT_FAILURE;
  }

  do {
     printf ("Command: ");
     fgets (buffer, BUF, stdin);    
     //printf("TEST: %s", buffer);
	if (strncmp (buffer, "SEND\n", 5) == 0 || strncmp(buffer, "send\n", 5) == 0){             //SEND
        if (send_okay(create_socket, buffer, strlen (buffer), 1) == 0){
			do {
				printf("Sender: ");
				fgets(sender, BUF, stdin);
			} while (send_okay(create_socket, sender, strlen(sender), 2) != 0);
			do {
				printf("Receiver: ");
				fgets(receiver, BUF, stdin);
			} while (send_okay(create_socket, receiver, strlen(receiver), 3) != 0);
			do {
				printf("Subject: ");
				fgets(subject, BUF, stdin);
			} while (send_okay(create_socket, subject, strlen(subject), 4) != 0);            

            if (errorc == 0){ 
				printf("Message: ");
				//strncmp("list", to_lower(del_new_line(buffer)), 4) == 0
				while(1) {
					fgets(buffer, BUF, stdin);
					send(create_socket, buffer, strlen(buffer), 0);
					if (strncmp(buffer, ".", 1) == 0){
						recv(create_socket, answer, BUF - 1, 0);
						break;
					}
				} 
				
				
            } 
            if (strncmp(answer, "OK\n", 3) == 0){
				system("clear");
                printf("Message sent!\n\n");
            }

        }else {
        printf("An error has occurred\n");
        }
        errorc = 0;
     }
  } 
  while (strcmp (buffer, "quit\n") != 0);
  close (create_socket);
  return EXIT_SUCCESS;
}

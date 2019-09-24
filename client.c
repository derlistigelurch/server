#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUF 1024

void print_usage();

int main(int argc, char *argv[])
{
    int c = 0;
    int option_p_counter = 0;
    int option_s_counter = 0;
    int port = 0;
    char *server_address;

    while((c = getopt(argc, argv, "p:s:")) != EOF)
    {
        switch(c)
        {
            case 'p':
                if(option_p_counter)
                {
                    print_usage();
                }
                port = (int) strtol(optarg, NULL, 10);
                option_p_counter = 1;
                break;
            case 's':
                if(option_s_counter)
                {
                    print_usage();
                }
                server_address = optarg;
                option_s_counter = 1;
                break;
            default:
                print_usage();
        }
    }
    if(argc < 2 || argc > 5 || option_p_counter != 1 || option_s_counter != 1)
    {
        print_usage();
    }

    int create_socket;
    char buffer[BUF];
    struct sockaddr_in address;
    int size;

    if((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket error");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    inet_aton(server_address, &address.sin_addr);

    if(connect(create_socket, (struct sockaddr *) &address, sizeof(address)) == 0)
    {
        printf("Connection with server (%s) established\n", inet_ntoa(address.sin_addr));
        size = recv(create_socket, buffer, BUF, 0);
        if(size > 0)
        {
            printf("%s", buffer);
        }
    }
    else
    {
        perror("Connect error - no server available");
        return EXIT_FAILURE;
    }
    do
    {
        fgets(buffer, BUF, stdin);
        send(create_socket, buffer, strlen(buffer), 0);
    } while(strcmp(buffer, "QUIT\n") != 0);
    close(create_socket);
    return EXIT_SUCCESS;
}

void print_usage()
{
    printf("Usage: client SERVERADDRESS PORT\n");
}

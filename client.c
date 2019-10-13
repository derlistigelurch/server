#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define BUF 1024
#define SENDER_ERROR 1
#define RECIPIENT_ERROR 2
#define SUBJECT_ERROR 3
#define USERNAME_ERROR 1
#define MAIL_NOT_FOUND_ERROR 2
#define MAIL_DIR_IS_EMPTY 3

int create_socket;

void print_usage();

char *to_lower(char *buffer);

ssize_t writen(int fd, const void *vptr, size_t n);

int check_receive(int size);

int send_error_check(int error_code);

int list_error_check();

int read_del_error_check(int error_code);

int main(int argc, char *argv[])
{
    char buffer[BUF];
    struct sockaddr_in address;
    int size;

    if(argc < 3)
    {
        print_usage();
    }

    if((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket error");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(strtol(argv[2], NULL, 10));
    inet_aton(argv[1], &address.sin_addr);

    if(connect(create_socket, (struct sockaddr *) &address, sizeof(address)) == 0)
    {
        fprintf(stdout, "Connection with server (%s) established\n", inet_ntoa(address.sin_addr));
        size = recv(create_socket, buffer, BUF - 1, 0);
        if(size > 0)
        {
            buffer[size] = '\0';
            fprintf(stdout, "%s", buffer);
        }
    }
    else
    {
        perror("Connect error - no server available\n");
        exit(EXIT_FAILURE);
    }
    do
    {
        memset(buffer, 0, sizeof(buffer));
        fprintf(stdout, "Command: ");
        fgets(buffer, BUF, stdin);

        if(strncmp(to_lower(buffer), "send\n", 5) == 0)
        {
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
            {
                if(strncmp(buffer, "OK\n", 3) == 0)
                {
                    do
                    {
                        fprintf(stdout, "Sender: ");
                        fgets(buffer, BUF, stdin);
                        if(writen(create_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                        memset(buffer, 0, sizeof(buffer));
                    }
                    while(send_error_check(SENDER_ERROR) != 0);
                    do
                    {
                        fprintf(stdout, "Recipient: ");
                        fgets(buffer, BUF, stdin);
                        if(writen(create_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                        memset(buffer, 0, sizeof(buffer));
                    }
                    while(send_error_check(RECIPIENT_ERROR) != 0);
                    do
                    {
                        fprintf(stdout, "Subject: ");
                        fgets(buffer, BUF, stdin);
                        if(writen(create_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                        memset(buffer, 0, sizeof(buffer));
                    }
                    while(send_error_check(SUBJECT_ERROR) != 0);
                    fprintf(stdout, "Content:\n");
                    do
                    {
                        fgets(buffer, BUF, stdin);
                        if(writen(create_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                    }
                    while(!((buffer[0] == '.' && buffer[1] == '\n')));
                    if(check_receive(recv(create_socket, buffer, BUF - 1, 0)) == 0)
                    {
                        if(strncmp(buffer, "OK\n", 3) == 0)
                        {
                            fprintf(stdout, "Message sent!\n");
                        }
                        else
                        {
                            fprintf(stderr, "ERROR! Unable to send message!\n");
                        }
                    }
                }
            }
        }
        else if(strncmp(to_lower(buffer), "list\n", 5) == 0)
        {
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
            {
                do
                {
                    fprintf(stdout, "Username: ");
                    fgets(buffer, BUF, stdin);
                    if(writen(create_socket, buffer, strlen(buffer)) < 0)
                    {
                        perror("send error");
                        exit(EXIT_FAILURE);
                    }
                    memset(buffer, 0, sizeof(buffer));
                }
                while(list_error_check() != 0);
                if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
                {
                    if(strncmp(buffer, "ERR\n", 4) == 0)
                    {
                        fprintf(stderr, "Something went terribly wrong");
                    }
                    else
                    {
                        fprintf(stdout, "%s", buffer);
                    }
                }
            }
        }
        else if(strncmp(to_lower(buffer), "read\n", 5) == 0)
        {
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
            {
                do
                {
                    fprintf(stdout, "Username: ");
                    fgets(buffer, BUF, stdin);
                    if(writen(create_socket, buffer, strlen(buffer)) < 0)
                    {
                        perror("send error");
                        exit(EXIT_FAILURE);
                    }
                    memset(buffer, 0, sizeof(buffer));
                }
                while(read_del_error_check(USERNAME_ERROR) != 0);
                if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
                {
                    do
                    {
                        fprintf(stdout, "Number: ");
                        fgets(buffer, BUF, stdin);
                        if(writen(create_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                        memset(buffer, 0, sizeof(buffer));
                    }
                    while(read_del_error_check(MAIL_NOT_FOUND_ERROR) != 0);
                    while(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) != 0)
                    {
                        fprintf(stdout, "%s", buffer);
                        memset(buffer, 0, sizeof(buffer));
                    }
                }
                else
                {
                    fprintf(stderr, "ERROR! Mail directory is EMPTY!\n");
                }
            }
        }
        else if(strncmp(to_lower(buffer), "del\n", 4) == 0)
        {
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
            {
                do
                {
                    fprintf(stdout, "Username: ");
                    fgets(buffer, BUF, stdin);
                    if(writen(create_socket, buffer, strlen(buffer)) < 0)
                    {
                        perror("send error");
                        exit(EXIT_FAILURE);
                    }
                    memset(buffer, 0, sizeof(buffer));
                }
                while(read_del_error_check(USERNAME_ERROR) != 0);
                if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
                {
                    do
                    {
                        fprintf(stdout, "Number: ");
                        fgets(buffer, BUF, stdin);
                        if(writen(create_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                        memset(buffer, 0, sizeof(buffer));
                    }
                    while(read_del_error_check(MAIL_NOT_FOUND_ERROR) != 0);
                    if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
                    {
                        fprintf(stdout, "Message deleted!\n");
                    }
                    else
                    {
                        fprintf(stderr, "ERROR! Unable to delete message!\n");
                    }
                }
                else
                {
                    fprintf(stderr, "ERROR! Mail directory is EMPTY!\n");
                }
            }
        }
    }
    while(strcmp(to_lower(buffer), "quit\n") != 0);
    close(create_socket);
    return EXIT_SUCCESS;
}

void print_usage()
{
    fprintf(stderr, "Usage: client SERVERADDRESS PORT\n");
    exit(EXIT_FAILURE);
}

int send_error_check(int error_code)
{
    char buffer[BUF];
    if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
    {
        if(strncmp(buffer, "OK\n", 3) == 0)
        {
            return EXIT_SUCCESS;
        }
        switch(error_code)
        {
            case SENDER_ERROR:
                fprintf(stderr, "ERROR! The SENDER must not have more than 8 characters!\n");
                return EXIT_FAILURE;
            case RECIPIENT_ERROR:
                fprintf(stderr, "ERROR! The RECIPIENT must not have more than 8 characters!\n");
                return EXIT_FAILURE;
            case SUBJECT_ERROR:
                fprintf(stderr, "ERROR! The SUBJECT must not have more than 80 characters!\n");
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Something went terribly wrong!\n");
                exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_FAILURE);
}

int list_error_check()
{
    char buffer[BUF];
    memset(buffer, 0, sizeof(buffer));
    if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
    {
        if(strncmp(buffer, "OK\n", 3) == 0)
        {
            return EXIT_SUCCESS;
        }
        fprintf(stderr, "ERROR! USERNAME not found!\n");
        return EXIT_FAILURE;
    }
    exit(EXIT_FAILURE);
}

int read_del_error_check(int error_code)
{
    char buffer[BUF];
    if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
    {
        if(strncmp(buffer, "OK\n", 3) == 0)
        {
            return EXIT_SUCCESS;
        }
        switch(error_code)
        {
            case USERNAME_ERROR:
                fprintf(stderr, "ERROR! USERNAME not found!\n");
                return EXIT_FAILURE;
            case MAIL_NOT_FOUND_ERROR:
                fprintf(stderr, "ERROR! MAIL not found!\n");
                return EXIT_FAILURE;
            case MAIL_DIR_IS_EMPTY:
                fprintf(stderr, "ERROR! MAIL not found!\n");
                return MAIL_DIR_IS_EMPTY;
            default:
                fprintf(stderr, "Something went terribly wrong!\n");
                exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_FAILURE);
}

char *to_lower(char *buffer)
{
    for(int i = 0; (int) i < strlen(buffer); i++)
    {
        buffer[i] = (char) tolower(buffer[i]);
    }
    return buffer;
}

// write n bytes to a descriptor
ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
    ptr = vptr;
    nleft = n;
    while(nleft > 0)
    {
        if((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if(errno == EINTR)
            {
                nwritten = 0; // and call write() again
            }
            else
            {
                return -1;
            }
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

int check_receive(int size)
{
    if(size > 0)
    {
        return EXIT_SUCCESS;
    }
    else if(size < 0)
    {
        perror("recv error");
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(stdout, "Lost connection to server\n");
        exit(EXIT_FAILURE);
    }
}

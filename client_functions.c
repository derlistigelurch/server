#include "client_functions.h"

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
                fprintf(stderr, "ERROR! The SENDER must be between 1 and 8 characters long!\n");
                return EXIT_FAILURE;
            case RECIPIENT_ERROR:
                fprintf(stderr, "ERROR! The RECIPIENT must be between 1 and 8 characters long!\n");
                return EXIT_FAILURE;
            case SUBJECT_ERROR:
                fprintf(stderr, "ERROR! The SUBJECT be between 1 and 80 characters long!\n");
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Something went terribly wrong!\n");
                exit(EXIT_FAILURE);
        }
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

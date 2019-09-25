#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <signal.h>

#define BUF 1024

struct Message
{
    char sender[9];
    char recipient[9];
    char subject[81];
};

int create_socket;

void sigquit_handler();

void sigint_handler();

void sighup_handler();

void print_usage();

int check_receive(int size);

ssize_t readline(int fd, void *vptr, size_t maxlen);

static ssize_t my_read(int fd, char *ptr);

ssize_t writen(int fd, const void *vptr, size_t n);

int get_mail_count(char *path);

int main(int argc, char *argv[])
{
    (void) signal(SIGHUP, sighup_handler);
    (void) signal(SIGQUIT, sigquit_handler);
    (void) signal(SIGINT, sigint_handler);

    if(argc != 3)
    {
        print_usage();
    }

    int new_socket;
    int port = (int) strtol(argv[1], NULL, 10);
    char dir_path[PATH_MAX];
    strcpy(dir_path, argv[2]);
    if(mkdir(dir_path, 0777) && errno != EEXIST)
    {
        perror("error: ");
        exit(EXIT_FAILURE);
    }

    socklen_t address_length;
    char buffer[BUF];
    int size;
    struct sockaddr_in address, client_address;

    create_socket = socket(AF_INET, SOCK_STREAM, 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if(bind(create_socket, (struct sockaddr *) &address, sizeof(address)) != 0)
    {
        perror("bind error");
        return EXIT_FAILURE;
    }

    listen(create_socket, 5);
    address_length = sizeof(struct sockaddr_in);

    while(!0)
    {
        printf("Waiting for connections...\n");
        new_socket = accept(create_socket, (struct sockaddr *) &client_address, &address_length);
        if(new_socket > 0)
        {
            printf("Client connected from %s:%d...\n", inet_ntoa(client_address.sin_addr),
                   ntohs(client_address.sin_port));
            strcpy(buffer, "Welcome to the server, Please enter your command:\n\0");
            send(new_socket, buffer, strlen(buffer) + 1, 0);
        }
        do
        {
            size = readline(new_socket, buffer, BUF);
            if(!check_receive(size))
            {
                if(strncmp("SEND", buffer, 4) == 0)
                {
                    struct Message message;
                    for(int i = 0; i < 3; i++)
                    {
                        size = readline(new_socket, buffer, BUF);
                        if(check_receive(size))
                        {
                            break;
                        }
                        switch(i)
                        {
                            case 0:
                                strncpy(message.sender, buffer, 8);
                                break;
                            case 1:
                                strncpy(message.recipient, buffer, 8);
                                break;
                            case 2:
                                strncpy(message.subject, buffer, 80);
                                break;
                            default:
                                exit(EXIT_FAILURE);
                        }
                    }
                    char temp_path[PATH_MAX];
                    strcpy(temp_path, dir_path);
                    strcat(temp_path, "/");
                    strcat(temp_path, message.recipient);
                    if(mkdir(temp_path, 0777) && errno != EEXIST)
                    {
                        perror("Error while creating a directory");
                        exit(EXIT_FAILURE);
                    }
                    strcat(temp_path, "/");
                    strcat(temp_path, message.sender);

                    FILE *file;
                    if((file = fopen(temp_path, "w")) == NULL)
                    {
                        if(writen(new_socket, "ERR\n\0", 5) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                    }
                    fprintf(file, "%s\n", message.sender);
                    fprintf(file, "%s\n", message.recipient);
                    fprintf(file, "%s", message.subject);
                    char temp;
                    char eof_string[3];
                    do
                    {
                        temp = buffer[size - 1];
                        size = readline(new_socket, buffer, BUF);
                        if(check_receive(size))
                        {
                            break;
                        }
                        fprintf(file, "%s", buffer);
                        strncpy(eof_string, &temp, 1);
                        eof_string[1] = buffer[0];
                        eof_string[2] = buffer[1];
                    }
                    while(strstr(eof_string, "\n.\n") == NULL && strstr(eof_string, "\n.\r") == NULL);
                    fclose(file);
                    if(writen(new_socket, "OK\n\0", 4) < 0)
                    {
                        perror("send error");
                        exit(EXIT_FAILURE);
                    }
                }
                else if(strncmp("LIST", buffer, 4) == 0)
                {
                    size = readline(new_socket, buffer, BUF);
                    if(check_receive(size))
                    {
                        break;
                    }
                    char temp_path[PATH_MAX];
                    strcpy(temp_path, dir_path);
                    strcat(temp_path, "/");
                    strcat(temp_path, buffer);
                    for(unsigned long i = 0; i < strlen(temp_path); i++)
                    {
                        if(temp_path[i] == '\0')
                        {
                            break;
                        }
                        if(temp_path[i] == '\n' || temp_path[i] == '\r')
                        {
                            temp_path[i] = '\0';
                        }
                    }

                    DIR *dir;
                    struct dirent *dir_entry;

                    if((dir = opendir(temp_path)) == NULL)
                    {
                        if(writen(new_socket, "ERR\n\0", 5) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                    }
                    else
                    {
                        snprintf(buffer, BUF, "%d\n", get_mail_count(temp_path));
                        if(writen(new_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }

                        char subjects[BUF];
                        while((dir_entry = readdir(dir)) != NULL)
                        {
                            if(!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, ".."))
                            {
                                continue;
                            }
                            FILE *file;
                            char line[BUF];
                            char path[PATH_MAX];
                            strncpy(path, temp_path, strlen(temp_path));
                            path[strlen(temp_path)] = '\0';
                            strcat(path, "/");
                            strcat(path, dir_entry->d_name);
                            if((file = fopen(path, "r")) != NULL)
                            {
                                for(int i = 0; i < 3; i++)
                                {
                                    fgets(line, BUF, file);
                                }
                                strcat(subjects, line);
                                fclose(file);
                                path[strlen(temp_path)] = '\0';
                            }
                            else
                            {
                                if(writen(new_socket, "ERR\n\0", 5) < 0)
                                {
                                    perror("send error");
                                    exit(EXIT_FAILURE);
                                }
                            }
                        }
                        if(writen(new_socket, subjects, strlen(subjects)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                        subjects[0] = '\0';
                    }
                }
                else if(strncmp("READ", buffer, 4) == 0)
                {
                    size = readline(new_socket, buffer, BUF);
                    if(check_receive(size))
                    {
                        break;
                    }
                    char temp_path[PATH_MAX];
                    strcpy(temp_path, dir_path);
                    strcat(temp_path, "/");
                    strcat(temp_path, buffer);
                    for(unsigned long i = 0; i < strlen(temp_path); i++)
                    {
                        if(temp_path[i] == '\0')
                        {
                            break;
                        }
                        if(temp_path[i] == '\n' || temp_path[i] == '\r')
                        {
                            temp_path[i] = '\0';
                        }
                    }
                    DIR *dir;
                    struct dirent *dir_entry;

                    if((dir = opendir(temp_path)) == NULL)
                    {
                        if(writen(new_socket, "ERR\n\0", 5) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                    }
                    else
                    {
                        size = readline(new_socket, buffer, BUF);
                        if(check_receive(size))
                        {
                            break;
                        }
                        int message_number = (int) strtol(buffer, NULL, 10);
                        int current_message_number = 1;
                        while((dir_entry = readdir(dir)) != NULL)
                        {
                            if(!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, ".."))
                            {
                                continue;
                            }
                            if(current_message_number == message_number)
                            {
                                FILE *file;
                                char line[BUF];
                                char path[PATH_MAX];
                                path[strlen(temp_path)] = '\0';
                                strncpy(path, temp_path, strlen(temp_path));
                                strcat(path, "/");
                                strcat(path, dir_entry->d_name);
                                if((file = fopen(path, "r")) != NULL)
                                {
                                    if(writen(new_socket, "OK\n\0", 4) < 0)
                                    {
                                        perror("send error");
                                        exit(EXIT_FAILURE);
                                    }
                                    while(fgets(line, BUF, file) != NULL)
                                    {
                                        if(writen(new_socket, line, strlen(line)) < 0)
                                        {
                                            perror("send error");
                                            exit(EXIT_FAILURE);
                                        }
                                    }
                                    fclose(file);
                                    break;
                                }
                            }
                            else
                            {
                                current_message_number++;
                            }
                        }
                        if(current_message_number != message_number)
                        {
                            if(writen(new_socket, "ERR\n\0", 5) < 0)
                            {
                                perror("send error");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                }
                else if(strncmp("DEL", buffer, 3) == 0)
                {
                    size = readline(new_socket, buffer, BUF);
                    if(check_receive(size))
                    {
                        break;
                    }
                    char temp_path[PATH_MAX];
                    strcpy(temp_path, dir_path);
                    strcat(temp_path, "/");
                    strcat(temp_path, buffer);
                    for(unsigned long i = 0; i < strlen(temp_path); i++)
                    {
                        if(temp_path[i] == '\0')
                        {
                            break;
                        }
                        if(temp_path[i] == '\n' || temp_path[i] == '\r')
                        {
                            temp_path[i] = '\0';
                        }
                    }
                    DIR *dir;
                    struct dirent *dir_entry;

                    if((dir = opendir(temp_path)) == NULL)
                    {
                        if(writen(new_socket, "ERR\n\0", 5) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                    }
                    else
                    {
                        size = readline(new_socket, buffer, BUF);
                        if(check_receive(size))
                        {
                            break;
                        }
                        int message_number = (int) strtol(buffer, NULL, 10);
                        int current_message_number = 1;
                        while((dir_entry = readdir(dir)) != NULL)
                        {
                            if(!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, ".."))
                            {
                                continue;
                            }
                            if(current_message_number == message_number)
                            {
                                char path[PATH_MAX];
                                strncpy(path, temp_path, strlen(temp_path));
                                path[strlen(temp_path)] = '\0';
                                strcat(path, "/");
                                strcat(path, dir_entry->d_name);
                                if(remove(path) == 0)
                                {
                                    if(writen(new_socket, "OK\n\0", 4) < 0)
                                    {
                                        perror("send error");
                                        exit(EXIT_FAILURE);
                                    }
                                    break;
                                }
                                else
                                {
                                    if(writen(new_socket, "ERR\n\0", 5) < 0)
                                    {
                                        perror("send error");
                                        exit(EXIT_FAILURE);
                                    }
                                    break;
                                }
                            }
                            current_message_number++;
                        }
                        if(current_message_number != message_number)
                        {
                            if(writen(new_socket, "ERR\n\0", 5) < 0)
                            {
                                perror("send error");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                }
            }
            else
            {
                return (EXIT_FAILURE);
            }
        }
        while(strncmp(buffer, "QUIT", 4) != 0);
        close(new_socket);
    }
}

void sigquit_handler()
{
    close(create_socket);
    exit(EXIT_SUCCESS);
}

void sigint_handler()
{
    close(create_socket);
    exit(EXIT_SUCCESS);
}

void sighup_handler()
{
    close(create_socket);
    exit(EXIT_SUCCESS);
}

int get_mail_count(char *path)
{
    DIR *dir = opendir(path);
    struct dirent *dir_entry;
    int mail_counter = 0;

    while((dir_entry = readdir(dir)) != NULL)
    {
        if(!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, ".."))
        {
            continue;
        }
        mail_counter++;
    }
    return mail_counter;
}

void print_usage()
{
    printf("Usage: server PORT DIRECTORY\n");
    exit(EXIT_FAILURE);
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
    return EXIT_FAILURE;
}

static ssize_t my_read(int fd, char *ptr)
{
    static int read_cnt = 0;
    static char *read_ptr;
    static char read_buf[BUF];
    if(read_cnt <= 0)
    {
        again:
        if((read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0)
        {
            if(errno == EINTR)
            {
                goto again;
            }
            return -1;
        }
        else if(read_cnt == 0)
        {
            return 0;
        }
        read_ptr = read_buf;
    };
    read_cnt--;
    *ptr = *read_ptr++;
    return 1;
}

ssize_t readline(int fd, void *vptr, size_t maxlen)
{
    ssize_t n, rc;
    char c, *ptr;
    ptr = vptr;
    for(n = 1; (long) n < maxlen; n++)
    {
        if((rc = my_read(fd, &c)) == 1)
        {
            *ptr++ = c;
            if(c == '\n')
            {
                break; // newline is stored
            }
        }
        else if(rc == 0)
        {
            if(n == 1)
            {
                return 0; // EOF, no data read
            }
            else
            {
                break; // EOF, some data was read
            }
        }
        else
        {
            return -1; // error, errno set by read() in my_read()
        }
    };
    *ptr = 0;
    return n; // null terminate
}

// write n bytes to a descriptor ...
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
    };
    return n;
}


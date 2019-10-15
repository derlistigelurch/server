#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#define BUF 1024
#define MAX_SENDER_SIZE 8
#define MAX_RECIPIENT_SIZE 8
#define MAX_SUBJECT_SIZE 80
#define SENDER 1
#define RECIPIENT 2
#define SUBJECT 3
#define CONTENT 4

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

void send_ok(int new_socket);

void send_err(int new_socket);

char *del_new_line(char *buffer);

char *get_user_dir_path(char *mail_dir_path, char *username);

char *to_lower(char *buffer);

int del_message(DIR *dir, char *user_dir_path, int message_number);

char *read_message(DIR *dir, char *user_dir_path, int message_number);

char *list_messages(DIR *dir, char *user_dir_path);

int main(int argc, char *argv[])
{
    (void) signal(SIGHUP, sighup_handler);
    (void) signal(SIGQUIT, sigquit_handler);
    (void) signal(SIGINT, sigint_handler);

    if(argc != 3)
    {
        print_usage();
    }

    time_t current_time;
    FILE *file = NULL;
    DIR *dir = NULL;
    int new_socket = 0;
    char mail_dir_path[PATH_MAX];
    char *user_dir_path = NULL;
    strncpy(mail_dir_path, argv[2], strlen(argv[2]));
    int value = 1;
    bool cancel = false;
    // create mail spool directory
    if(mkdir(mail_dir_path, 0777) && errno != EEXIST) // do nothing if directory already exists
    {
        perror("mkdir error: ");// error while creating a directory
        exit(EXIT_FAILURE);
    }

    
    socklen_t address_length;
    char buffer[BUF];
    int size = 0;
    struct sockaddr_in address;
    struct sockaddr_in client_address;

    if((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    // set SO_REUSEADDR in create_socket to 1 to prevent the "Address already in use" error
    if(setsockopt(create_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address)); // zero the rest of the struct
    address.sin_family = AF_INET; // use TCP
    address.sin_addr.s_addr = htonl(INADDR_ANY); // use my IP address
    // all ports below 1024 are reserved
    address.sin_port = htons(strtol(argv[1], NULL, 10)); // use argv[1] as my port

    if(bind(create_socket, (struct sockaddr *) &address, sizeof(address)) != 0)
    {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    listen(create_socket, 5);
    address_length = sizeof(struct sockaddr_in);

    while(1)
    {
        new_socket = accept(create_socket, (struct sockaddr *) &client_address, &address_length);

        if(new_socket > 0)
        {
            fprintf(stdout, "Client connected from %s:%d...\n", inet_ntoa(client_address.sin_addr),
                    ntohs(client_address.sin_port));
            strcpy(buffer,
                   "Welcome to the server\n\n\0");// Please enter your command:\nSEND\nLIST\nREAD\nDEL\nQUIT\n\0");
            if(writen(new_socket, buffer, strlen(buffer) + 1) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
        }
        do
        {
            if((size = check_receive(readline(new_socket, buffer, BUF))) == 0)
            {
                cancel = false;
                if(strncmp("send\n", to_lower(buffer), 5) == 0 || strncmp("send\r", to_lower(buffer), 5) == 0)
                {
                    struct Message message;
                    int i = 1;
                    send_ok(new_socket);

                    while(i < 4)
                    {
                        if((size = check_receive(readline(new_socket, buffer, BUF))) == 0)
                        {
                            switch(i)
                            {
                                case SENDER:
                                    if(strlen(del_new_line(buffer)) > MAX_SENDER_SIZE || strlen(del_new_line(buffer)) == 0)
                                    {
                                        send_err(new_socket);
                                        break;
                                    }
                                    strncpy(message.sender, buffer, 8);
                                    send_ok(new_socket);
                                    i++;
                                    break;
                                case RECIPIENT:
                                    if(strlen(del_new_line(buffer)) > MAX_RECIPIENT_SIZE || strlen(del_new_line(buffer)) == 0)
                                    {
                                        send_err(new_socket);
                                        break;
                                    }
                                    strncpy(message.recipient, buffer, 8);
                                    send_ok(new_socket);
                                    i++;
                                    break;
                                case SUBJECT:
                                    if(strlen(del_new_line(buffer)) > MAX_SUBJECT_SIZE || strlen(del_new_line(buffer)) == 0)
                                    {
                                        send_err(new_socket);
                                        break;
                                    }
                                    strncpy(message.subject, buffer, 80);
                                    i++;
                                    send_ok(new_socket);
                                    break;
                                default:
                                    exit(EXIT_FAILURE);
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                    if(size == 0)
                    {
                        user_dir_path = get_user_dir_path(mail_dir_path, message.recipient);

                        if(mkdir(user_dir_path, 0777) && errno != EEXIST)
                        {
                            perror("error");
                            exit(EXIT_FAILURE);
                        }

                        strcat(user_dir_path, "/");
                        current_time = time(NULL);
                        snprintf(buffer, BUF, "%ld_%s", current_time, message.sender);
                        strncat(user_dir_path, buffer, strlen(buffer));

                        if((file = fopen(del_new_line(user_dir_path), "w")) == NULL)
                        {
                            send_err(new_socket);
                        }
                        else
                        {
                            fprintf(file, "%s\n", message.sender);
                            fprintf(file, "%s\n", message.recipient);
                            fprintf(file, "%s\n", message.subject);
                            do
                            {
                                if((size = check_receive(readline(new_socket, buffer, BUF))))
                                {
                                    break;
                                }

                                fprintf(file, "%s", buffer);
                            }
                            while(!((buffer[0] == '.' && (buffer[1] == '\n' || buffer[1] == '\r'))));
                            fclose(file);
                            if(size)
                            {
                                if(remove(user_dir_path) != 0)
                                {
                                    perror("del error");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            else
                            {
                                send_ok(new_socket);
                            }
                        }
                    }
                }
                else if(strncmp("list\n", to_lower(buffer), 5) == 0 ||
                        strncmp("list\r", to_lower(buffer), 5) == 0)
                {
                    send_ok(new_socket);
                    do
                    {
                        if((size = check_receive(readline(new_socket, buffer, BUF))))
                        {
                            break;
                        }
                        if (buffer[0] == '.' || buffer[0] == '/')
                        {
                            send_err(new_socket);
                            continue;
                        }
                        if (strlen(buffer) == 1)
                        {
                            cancel = true;
                            break;
                        }
                        user_dir_path = get_user_dir_path(mail_dir_path, del_new_line(buffer));

                        if((dir = opendir(del_new_line(user_dir_path))) == NULL)
                        {
                            send_err(new_socket);
                        }
                    }
                    while(dir == NULL);
                    send_ok(new_socket);
                    // abbrechen, wenn leerstring -> zurück zum hauptmenü
                    if (cancel) {
                        continue;
                    }

                    char *subjects = NULL;
                    if((subjects = list_messages(dir, user_dir_path)) != NULL)
                    {
                        if(writen(new_socket, subjects, strlen(subjects)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                    }
                    else
                    {
                        send_err(new_socket);
                    }

                }
                else if(strncmp("read\n", to_lower(buffer), 5) == 0 || (strncmp("read\r", to_lower(buffer), 5) == 0))
                {
                    send_ok(new_socket);
                    do
                    {
                        if((size = check_receive(readline(new_socket, buffer, BUF))))
                        {
                            break;
                        }
                        //errorhandling, Pfadunsinn
                        if (buffer[0] == '.' || buffer[0] == '/')
                        {
                            send_err(new_socket);
                            continue;
                        }
                        if (strlen(buffer) == 1)
                        {
                            cancel = true;
                            break;
                        }
                        user_dir_path = get_user_dir_path(mail_dir_path, buffer);
                        

                        if((dir = opendir(del_new_line(user_dir_path))) == NULL)
                        {
                            send_err(new_socket);
                        }
                    }
                    while(dir == NULL);

                    send_ok(new_socket);
                    // abbrechen, wenn leerstring -> zurück zum hauptmenü
                    if (cancel) {
                        continue;
                    }

                    
                    int message_number_is_valid = 0;
                    int message_number = 1;

                    if(get_mail_count(user_dir_path) == 0)
                    {
                        send_err(new_socket);
                        break;
                    }
                    else
                    {
                        send_ok(new_socket);

                        do
                        {
                            if((size = check_receive(readline(new_socket, buffer, BUF))))
                            {
                                break;
                            }
                            message_number = (int) strtol(buffer, NULL, 10);
                            if((message_number_is_valid = message_number > get_mail_count(user_dir_path) || message_number <= 0))
                            {
                                send_err(new_socket);
                            }
                        }
                        while(message_number_is_valid != 0);
                        send_ok(new_socket);

                        char *message = NULL;
                        if((message = read_message(dir, user_dir_path, message_number)) != NULL)
                        {
                            if(writen(new_socket, message, strlen(message)) < 0)
                            {
                                perror("send error");
                                exit(EXIT_FAILURE);
                            }
                        }
                        else
                        {
                            send_err(new_socket);
                        }
                    }
                }
                else if(strncmp("del\n", to_lower(buffer), 4) == 0 || strncmp("del\r", to_lower(buffer), 4) == 0)
                {
                    send_ok(new_socket);
                    do
                    {
                        if((size = check_receive(readline(new_socket, buffer, BUF))))
                        {
                            break;
                        }

                        user_dir_path = get_user_dir_path(mail_dir_path, buffer);

                        if((dir = opendir(del_new_line(user_dir_path))) == NULL)
                        {
                            send_err(new_socket);
                        }
                    }
                    while(dir == NULL);

                    send_ok(new_socket);
                    int message_number_is_valid = 0;
                    int message_number = 1;

                    if(get_mail_count(user_dir_path) == 0)
                    {
                        send_err(new_socket);
                    }
                    else
                    {
                        send_ok(new_socket);

                        do
                        {
                            if((size = check_receive(readline(new_socket, buffer, BUF))))
                            {
                                break;
                            }
                            message_number = (int) strtol(buffer, NULL, 10);
                            if((message_number_is_valid =
                                        message_number > get_mail_count(user_dir_path) ||
                                        message_number <= 0))
                            {
                                send_err(new_socket);
                            }
                        }
                        while(message_number_is_valid != 0);
                        send_ok(new_socket);

                        if(del_message(dir, user_dir_path, message_number) == 0)
                        {
                            send_ok(new_socket);
                        }
                        else
                        {
                            send_err(new_socket);
                        }
                    }
                }
            }
        }
        while((strncmp("quit\n", to_lower(buffer), 5) != 0 || strncmp("quit\r", to_lower(buffer), 5) != 0) && !size);
        close(new_socket);
        return EXIT_SUCCESS;
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

char *list_messages(DIR *dir, char *user_dir_path)
{
    static char buffer[BUF];
    char subjects[BUF];
    memset(buffer, 0, sizeof(buffer));
    memset(subjects, 0, sizeof(subjects));
    struct dirent *dir_entry = NULL;
    FILE *file = NULL;
    int counter = 1;

    snprintf(buffer, BUF, "%d\n", get_mail_count(user_dir_path));
    if(get_mail_count(user_dir_path) > 0)
    {
        while((dir_entry = readdir(dir)) != NULL)
        {
            if(!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, ".."))
            {
                continue;
            }

            char line[BUF];
            char temp_path[PATH_MAX];

            strncpy(temp_path, user_dir_path, strlen(user_dir_path));
            temp_path[strlen(user_dir_path)] = '\0';
            strcat(temp_path, "/");
            strcat(temp_path, dir_entry->d_name);

            if((file = fopen(temp_path, "r")) != NULL)
            {
                for(int i = 0; i < 3; i++)
                {
                    fgets(line, BUF, file);
                }

                fclose(file);

                temp_path[strlen(user_dir_path)] = '\0';
                snprintf(subjects, BUF, "%d.) %s", counter, line);
                strcat(buffer, subjects);
                counter++;
            }
            else
            {
                perror("Unable to open file\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return buffer;
}

char *read_message(DIR *dir, char *user_dir_path, int message_number)
{
    char temp_buffer[BUF];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    static char buffer[BUF];
    memset(buffer, 0, sizeof(buffer));
    struct dirent *dir_entry = NULL;
    FILE *file = NULL;
    int current_message_number = 1;
    while((dir_entry = readdir(dir)) != NULL)
    {
        if(!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, ".."))
        {
            continue;
        }
        if(current_message_number == message_number)
        {
            char line[BUF];
            char path[PATH_MAX];
            path[strlen(user_dir_path)] = '\0';
            strncpy(path, user_dir_path, strlen(user_dir_path));
            strcat(path, "/");
            strcat(path, dir_entry->d_name);

            if((file = fopen(path, "r")) != NULL)
            {
                int i = 1;
                memset(temp_buffer, 0, sizeof(temp_buffer));

                while(fgets(line, BUF, file) != NULL)
                {
                    switch(i)
                    {
                        case SENDER:
                            snprintf(temp_buffer, BUF, "\nFROM: %s", line);
                            break;
                        case RECIPIENT:
                            snprintf(temp_buffer, BUF, "TO: %s", line);
                            break;
                        case SUBJECT:
                            snprintf(temp_buffer, BUF, "SUBJECT: %s", line);
                            break;
                        case CONTENT:
                            snprintf(temp_buffer, BUF, "CONTENT:\n%s", line);
                            break;
                        default:
                            snprintf(temp_buffer, BUF, "%s", line);
                            break;
                    }
                    i++;
                    strncat(buffer, temp_buffer, strlen(temp_buffer));
                }
                fclose(file);
                return buffer;
            }
        }
        else
        {
            current_message_number++;
        }
    }
    if((current_message_number != message_number) || (dir_entry == NULL && message_number == 1))
    {
        return NULL;
    }
    return NULL;
}

int del_message(DIR *dir, char *user_dir_path, int message_number)
{
    char buffer[BUF];
    memset(buffer, 0, sizeof(buffer));
    struct dirent *dir_entry = NULL;
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
            strncpy(path, user_dir_path, strlen(user_dir_path));
            path[strlen(user_dir_path)] = '\0';
            strcat(path, "/");
            strcat(path, dir_entry->d_name);

            if(remove(path) == 0)
            {
                return EXIT_SUCCESS;
            }
            else
            {
                perror("del error");
                return EXIT_FAILURE;
            }
        }
        current_message_number++;
    }
    if((current_message_number != message_number) || (dir_entry == NULL && message_number == 1))
    {
        return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
}

void send_ok(int new_socket)
{
    if(send(new_socket, "OK\n\0", 4, 0) < 0)
    {
        perror("send error");
        exit(EXIT_FAILURE);
    }
}

void send_err(int new_socket)
{
    if(writen(new_socket, "ERR\n\0", 5) < 0)
    {
        perror("send error");
        exit(EXIT_FAILURE);
    }
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
    fprintf(stderr, "Usage: server PORT DIRECTORY\n");
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
    else
    {
        fprintf(stdout, "Client closed remote socket\n");
        return EXIT_FAILURE;
    }
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
    }
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
    }
    *ptr = 0;
    return n; // null terminate
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

char *del_new_line(char *buffer)
{
    for(unsigned long i = 0; i < strlen(buffer); i++)
    {
        if(buffer[i] == '\0')
        {
            break;
        }
        if(buffer[i] == '\n' || buffer[i] == '\r')
        {
            buffer[i] = '\0';
        }
    }
    return buffer;
}

// returns the path to the user mail directory
char *get_user_dir_path(char *mail_dir_path, char *username)
{
    static char user_dir_path[PATH_MAX];
    strncpy(user_dir_path, mail_dir_path, strlen(mail_dir_path));
    user_dir_path[strlen(mail_dir_path)] = '\0';
    strncat(user_dir_path, "/", 1);
    strncat(user_dir_path, username, strlen(username));

    return user_dir_path;
}

char *to_lower(char *buffer)
{
    for(int i = 0; (int) i < strlen(buffer); i++)
    {
        buffer[i] = (char) tolower(buffer[i]);
    }
    return buffer;
}

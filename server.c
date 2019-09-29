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
#include <time.h>
#include <ctype.h>

#define BUF 1024
#define MAX_SENDER_SIZE 8
#define MAX_RECIPIENT_SIZE 8
#define MAX_SUBJECT_SIZE 80

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
    FILE *file;
    DIR *dir;
    struct dirent *dir_entry;
    int new_socket;
    char mail_dir_path[PATH_MAX];
    char *user_dir_path = NULL;
    strncpy(mail_dir_path, argv[2], strlen(argv[2]));
    int value = 1;

    // create mail spool directory
    if(mkdir(mail_dir_path, 0777) && errno != EEXIST)
    {
        perror("mkdir error: ");
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
        return EXIT_FAILURE;
    }

    listen(create_socket, 5);
    address_length = sizeof(struct sockaddr_in);

    while(1)
    {
        fprintf(stdout, "Waiting for connections...\n");
        new_socket = accept(create_socket, (struct sockaddr *) &client_address, &address_length);
        if(new_socket > 0)
        {
            fprintf(stdout, "Client connected from %s:%d...\n", inet_ntoa(client_address.sin_addr),
                    ntohs(client_address.sin_port));
            strcpy(buffer, "Welcome to the server, Please enter your command:\n\0");
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
                if(strncmp("send", to_lower(del_new_line(buffer)), 4) == 0)
                {
                    struct Message message;
                    int i = 0;
                    while(i < 3)
                    {
                        if((size = check_receive(readline(new_socket, buffer, BUF))) == 0)
                        {
                            switch(i)
                            {
                                case 0:
                                    if(strlen(del_new_line(buffer)) > MAX_SENDER_SIZE)
                                    {
                                        send_err(new_socket);
                                        break;
                                    }
                                    strncpy(message.sender, buffer, 8);
                                    //send_ok(new_socket);
                                    i++;
                                    break;
                                case 1:
                                    if(strlen(del_new_line(buffer)) > MAX_RECIPIENT_SIZE)
                                    {
                                        send_err(new_socket);
                                        break;
                                    }
                                    strncpy(message.recipient, buffer, 8);
                                    //send_ok(new_socket);
                                    i++;
                                    break;
                                case 2:
                                    if(strlen(del_new_line(buffer)) > MAX_SUBJECT_SIZE)
                                    {
                                        send_err(new_socket);
                                        break;
                                    }
                                    strncpy(message.subject, buffer, 80);
                                    i++;
                                    //send_ok(new_socket);
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
                        snprintf(buffer, BUF, "%s_%ld", message.sender, current_time);
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
                            } while(!((buffer[0] == '.' && (buffer[1] == '\n' || buffer[1] == '\r'))));
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
                else if(strncmp("list", to_lower(del_new_line(buffer)), 4) == 0)
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
                    else
                    {
                        snprintf(buffer, BUF, "%d\n", get_mail_count(user_dir_path));
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
                                strcat(subjects, line);
                                fclose(file);
                                temp_path[strlen(user_dir_path)] = '\0';
                            }
                            else
                            {
                                send_err(new_socket);
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
                else if(strncmp("read", to_lower(del_new_line(buffer)), 4) == 0)
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
                    else
                    {
                        if((size = check_receive(readline(new_socket, buffer, BUF))))
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
                                char line[BUF];
                                char path[PATH_MAX];
                                path[strlen(user_dir_path)] = '\0';
                                strncpy(path, user_dir_path, strlen(user_dir_path));
                                strcat(path, "/");
                                strcat(path, dir_entry->d_name);
                                if((file = fopen(path, "r")) != NULL)
                                {
                                    send_ok(new_socket);
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
                        if((current_message_number != message_number) || (dir_entry == NULL && message_number == 1))
                        {
                            send_err(new_socket);
                        }
                    }
                }
                else if(strncmp("del", to_lower(del_new_line(buffer)), 3) == 0)
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
                    else
                    {
                        if((size = check_receive(readline(new_socket, buffer, BUF))))
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
                                strncpy(path, user_dir_path, strlen(user_dir_path));
                                path[strlen(user_dir_path)] = '\0';
                                strcat(path, "/");
                                strcat(path, dir_entry->d_name);

                                if(remove(path) == 0)
                                {
                                    send_ok(new_socket);
                                    break;
                                }
                                else
                                {
                                    perror("del error");
                                    send_err(new_socket);
                                    break;
                                }
                            }
                            current_message_number++;
                        }
                        if((current_message_number != message_number) || (dir_entry == NULL && message_number == 1))
                        {
                            send_err(new_socket);
                        }
                    }
                }
            }
        } while(strncmp("quit", to_lower(del_new_line(buffer)), 4) != 0 && size == 0);
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

void send_ok(int new_socket)
{
    if(writen(new_socket, "OK\n\0", 4) < 0)
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
    fprintf(stdout, "Usage: server PORT DIRECTORY\n");
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
    for(int i = 0; (int) i < strlen(buffer); ++i)
    {
        buffer[i] = (char) tolower(buffer[i]);
    }
    return buffer;
}

//  |──────────────────────────────────────────────────────────────────────────────────────────────────────────────────|
//  |──────────────────────────────────────────────────────────────▄▀▀▀▄▄▄▄▄▄▄▀▀▀▄─────────────────────────────────────|
//  |───────▄▀▄─────▄▀▄──────────────────▄▀▄─────▄▀▄───────────────█▒▒░░░░░░░░░▒▒█───────────────▄▀▄─────▄▀▄───────────|
//  |──────▄█░░▀▀▀▀▀░░█▄────────────────▄█░░▀▀▀▀▀░░█▄───────────────█░░█░░░░░█░░█───────────────▄█░░▀▀▀▀▀░░█▄──────────|
//  |──▄▄──█░░░░░░░░░░░█──▄▄────────▄▄──█░░░░░░░░░░░█──▄▄────────▄▄──█░░░▀█▀░░░█──▄▄────────▄▄──█░░░░░░░░░░░█──▄▄──────|
//  |─█▄▄█─█░░▀░░┬░░▀░░█─█▄▄█──────█▄▄█─█░░▀░░┬░░▀░░█─█▄▄█──────█░░█─▀▄░░░░░░░▄▀─█░░█──────█▄▄█─█░░▀░░┬░░▀░░█─█▄▄█─────|
//  ▐███████████████████████████████████████████████████████████████████████████████████████████████████████████████████

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
#include <pthread.h>
#include <uuid/uuid.h>

#define BUF 1024
#define MAX_RECIPIENT_SIZE 8
#define MAX_SUBJECT_SIZE 80
#define SENDER 1
#define RECIPIENT 2
#define SUBJECT 3
#define CONTENT 4
#define BAN_TIME 15
#define UUID_SIZE 37

int create_socket;
pthread_mutex_t mutex_mail;
pthread_mutex_t mutex_ip;

struct Message
{
    char sender[9];
    char recipient[9];
    char subject[81];
};

void sig_handler();

void print_usage();

int check_receive(int size);

ssize_t readline(int fd, void *vptr, size_t maxlen);

ssize_t my_read(int fd, char *ptr);

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

struct thread_parameter
{
    int socket;
    struct sockaddr_in client_address;
    char *mail_dir_path;
};

void *server_function(void *parameter);


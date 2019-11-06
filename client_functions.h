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
#include <stdbool.h>
#include <termios.h>

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

int read_del_error_check(int error_code);
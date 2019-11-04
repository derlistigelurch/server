#include "client_functions.h"

int main(int argc, char *argv[])
{
    char buffer[BUF];
    struct sockaddr_in address;
    int size;
    bool logged_in = false;
    struct termios term;
    struct termios term_orig;

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
        system("clear");
        fprintf(stdout, "Connection with server (%s) established\n", inet_ntoa(address.sin_addr));
        fflush(stdout);
        size = recv(create_socket, buffer, BUF - 1, 0);
        if(size > 0)
        {
            buffer[size] = '\0';
            fprintf(stdout, "%s", buffer);
            fflush(stdout);
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
        if(logged_in)
        {
            fprintf(stdout, "\n[send][list][read][del][logout][quit]\nCommand: ");
            fflush(stdout);
        }
        else
        {
            fprintf(stdout, "\n[login][quit]\nCommand: ");
            fflush(stdout);

        }

        fgets(buffer, BUF, stdin);
        if(strncmp(to_lower(buffer), "login\n", 5) == 0)
        {                                                        //LOGIN
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
            {
                if(strncmp(buffer, "ERR\n", 4) == 0)
                {
                    //system("clear");
                    fprintf(stdout, "\n You are blocked for the time being!\n");
                    fflush(stdout);
                    continue;
                }
            }

            fprintf(stdout, "Username: ");
            fflush(stdout);
            fgets(buffer, BUF, stdin);

            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }

            memset(buffer, 0, sizeof(buffer));
            fprintf(stdout, "Passwort: ");
            fflush(stdout);

            tcgetattr(STDIN_FILENO, &term);
            term_orig = term;
            term.c_lflag &= ~ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &term);
            fgets(buffer, BUF, stdin);

            //Echo wieder an; sonst keine ausgabe
            tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);

            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }

            memset(buffer, 0, sizeof(buffer));

            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
            {
                if(strncmp(buffer, "OK\n", 3) == 0)
                {
                    logged_in = true;
                    system("clear");
                    fprintf(stdout, "\nLogin success! \n");
                    fflush(stdout);
                }
                else
                {
                    fprintf(stderr, "\nLogin failed! \n");
                    fflush(stderr);
                }
            }
        }
        else if(strncmp(to_lower(buffer), "logout\n", 7) == 0 && logged_in)
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
                    logged_in = false;
                }
                else
                {
                    fprintf(stdout, "Please try again!\n");
                    fflush(stdout);
                }
            }
        }
        else if(strncmp(to_lower(buffer), "send\n", 5) == 0 &&
                logged_in)                                          //SEND
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
                        fprintf(stdout, "Recipient: ");
                        fflush(stdout);
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
                        fflush(stdout);
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
                    fflush(stdout);
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
                            system("clear");
                            fprintf(stdout, "\nMessage sent!\n\n");
                            fflush(stdout);
                        }
                        else
                        {
                            fprintf(stderr, "ERROR! Unable to send message!\n");
                            fflush(stderr);
                        }
                    }
                }
            }
        }
        else if(strncmp(to_lower(buffer), "list\n", 5) == 0 && logged_in)                                         //LIST
        {
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
            {
                if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
                {
                    if(strncmp(buffer, "ERR\n", 4) == 0)
                    {
                        fprintf(stderr, "Something went terribly wrong");
                        fflush(stderr);
                    }
                    else
                    {
                        fprintf(stdout, "%s", buffer);
                        fflush(stdout);
                    }
                }
            }
        }
        else if(strncmp(to_lower(buffer), "read\n", 5) == 0 && logged_in)                                        //READ
        {
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
            {
                if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
                {
                    do
                    {
                        fprintf(stdout, "Number: ");
                        fflush(stdout);
                        fgets(buffer, BUF, stdin);
                        if(writen(create_socket, buffer, strlen(buffer)) < 0)
                        {
                            perror("send error");
                            exit(EXIT_FAILURE);
                        }
                        memset(buffer, 0, sizeof(buffer));
                    }
                    while(read_del_error_check(MAIL_NOT_FOUND_ERROR) != 0);
                    if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0)
                    {
                        buffer[strlen(buffer) - 2] = '\0';
                        fprintf(stdout, "%s", buffer);
                        fflush(stdout);
                    }
                    else
                    {
                        fprintf(stderr, "ERROR! Unable to read message!\n");
                        fflush(stderr);
                    }
                }
                else
                {
                    fprintf(stderr, "ERROR! Mail directory is EMPTY!\n");
                    fflush(stderr);
                }
            }
        }
        else if(strncmp(to_lower(buffer), "del\n", 4) == 0 && logged_in)                                         //DEL
        {
            if(writen(create_socket, buffer, strlen(buffer)) < 0)
            {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
            {

                if(check_receive(recv(create_socket, buffer, BUF, 0)) == 0 && strncmp(buffer, "OK\n", 3) == 0)
                {
                    do
                    {
                        fprintf(stdout, "Number: ");
                        fflush(stdout);
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
                        system("clear");
                        fprintf(stdout, " \nMessage deleted!\n\n");
                        fflush(stdout);
                    }
                    else
                    {
                        fprintf(stderr, "ERROR! Unable to delete message!\n");
                        fflush(stderr);
                    }
                }
                else
                {
                    fprintf(stderr, "ERROR! Mail directory is EMPTY!\n");
                    fflush(stderr);
                }
            }
        }
    }
    while(strcmp(to_lower(buffer), "quit\n") != 0);
    close(create_socket);
    return EXIT_SUCCESS;
}

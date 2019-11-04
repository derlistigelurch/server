#include "server_functions.h"
#include "myldap.h"

void sig_handler()
{
    close(create_socket);
    pthread_mutex_destroy(&mutex_ip);
    pthread_mutex_destroy(&mutex_mail);
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

ssize_t my_read(int fd, char *ptr)
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

void *server_function(void *parameter)
{
    int new_socket = ((struct thread_parameter *) parameter)->socket;
    struct sockaddr_in client_address = ((struct thread_parameter *) parameter)->client_address;
    char *mail_dir_path = ((struct thread_parameter *) parameter)->mail_dir_path;

    int check_time = 0;
    char buffer[BUF];
    int size = 0;
    char ip_user[INET_ADDRSTRLEN];
    bool logged_in = false;
    char logged_in_user[9] = "";
    char password[30] = "";
    char *user_dir_path = NULL;
    char *user_ip_dir_path = NULL;
    char check_ip[30] = "";
    FILE *file = NULL;
    DIR *dir = NULL;

    //IP ADRESSE SPEICHERN
    struct sockaddr_in *pV4Address = (struct sockaddr_in *) &client_address;
    struct in_addr ipAddress = pV4Address->sin_addr;
    inet_ntop(AF_INET, &ipAddress, ip_user, INET_ADDRSTRLEN);
    //für späteres login
    user_ip_dir_path = get_user_dir_path("IP_Blacklist", ip_user);

    pthread_t tid;
    tid = pthread_self();

    strcpy(buffer, "Welcome to the server\n\n\0");
    if(writen(new_socket, buffer, strlen(buffer) + 1) < 0)
    {
        perror("send error");
        pthread_exit(NULL);
    }

    do
    {
        if((size = check_receive(readline(new_socket, buffer, BUF))) == 0)
        {
            fprintf(stdout, "[%ld]: Received command: %s", tid, buffer);
            if(strncmp("login\n", to_lower(buffer), 6) == 0 || strncmp("login\r", to_lower(buffer), 6) == 0)
            {
                pthread_mutex_lock(&mutex_ip);
                if(access(user_ip_dir_path, F_OK) != -1)
                {
                    // es gibt schon ein file; dann nachschauen ob schon 3ter versuch
                    file = fopen(user_ip_dir_path, "r");
                    if(file == NULL)
                    {
                        //printf("Da hats was\n");
                    }
                    else
                    {
                        if(fgetc(file) == '3')
                        {
                            //wenn schon drei mal falsch
                            fgetc(file);
                            fgets(check_ip, 30, file);
                            check_time = (int) strtol(check_ip, NULL, 10);
                            check_time -= time(NULL);
                            if(check_time < BAN_TIME)
                            {
                                //wenn genügend zeit vergangen, wieder login zulassen
                                remove(user_ip_dir_path);
                            }
                            else
                            {
                                send_err(new_socket);
                                pthread_mutex_unlock(&mutex_ip);
                                continue;
                            }
                        }
                        fclose(file);
                    }
                    send_ok(new_socket);
                }
                else
                {
                    send_ok(new_socket);
                }
                pthread_mutex_unlock(&mutex_ip);
                if((size = check_receive(readline(new_socket, buffer, BUF))) == 0)
                {
                    if(strlen(del_new_line(buffer)) > 30 || strlen(del_new_line(buffer)) == 0)
                    {
                        send_err(new_socket);
                        break;
                    }
                    sprintf(logged_in_user, "%s", buffer);
                    memset(buffer, 0, sizeof(buffer));
                }
                if((size = check_receive(readline(new_socket, buffer, BUF))) == 0)
                {
                    if(strlen(del_new_line(buffer)) > 30 || strlen(del_new_line(buffer)) == 0)
                    {
                        send_err(new_socket);
                        break;
                    }
                    sprintf(password, "%s", buffer);
                }
                if(ldap_login(logged_in_user, password) == 0)
                {
                    printf("[%ld]: login success\n", tid);
                    logged_in = true;
                    pthread_mutex_lock(&mutex_ip);
                    remove(user_ip_dir_path);
                    pthread_mutex_unlock(&mutex_ip);
                    send_ok(new_socket);
                }
                else
                {
                    pthread_mutex_lock(&mutex_ip);
                    if((file = fopen(user_ip_dir_path, "r")) != NULL)
                    {
                        check_ip[0] = (char) fgetc(file);
                        fclose(file);
                    }
                    else
                    { check_ip[0] = '0'; }
                    if((file = fopen(user_ip_dir_path, "w")) != NULL)
                    {
                        switch(check_ip[0])
                        {
                            case '0':
                                fprintf(file, "1:%ld\n", time(NULL));
                                break;
                            case '1':
                                fprintf(file, "2:%ld\n", time(NULL));
                                break;
                            case '2':
                            {
                                fprintf(file, "3:%ld\n", time(NULL));
                                //ev Warnung ausgeben
                                break;
                            }
                            default:
                                fprintf(file, "3:%ld\n", time(NULL));
                                break;
                        }
                        fclose(file);
                    }
                    pthread_mutex_unlock(&mutex_ip);
                    printf("[%ld]: login error\n", tid);
                    send_err(new_socket);
                }
            }
            else if((strncmp("logout\n", to_lower(buffer), 7) == 0 || strncmp("logout\r", to_lower(buffer), 7) == 0) &&
                    logged_in)
            {
                logged_in = false;
                memset(logged_in_user, 0, sizeof(logged_in_user));
                memset(password, 0, sizeof(password));
                send_ok(new_socket);
            }
            else if((strncmp("send\n", to_lower(buffer), 5) == 0 || strncmp("send\r", to_lower(buffer), 5) == 0) &&
                    logged_in)
            {
                struct Message message;
                int i = 2;
                send_ok(new_socket);

                while(i < 4)
                {
                    if((size = check_receive(readline(new_socket, buffer, BUF))) == 0)
                    {
                        strncpy(message.sender, logged_in_user, 9);
                        switch(i)
                        {
                            case RECIPIENT:
                                if(strlen(del_new_line(buffer)) > MAX_RECIPIENT_SIZE ||
                                   strlen(del_new_line(buffer)) == 0)
                                {
                                    send_err(new_socket);
                                    break;
                                }
                                //snprintf(filePath,filePathSize,"%s%s%c",path.c_str(),s.c_str(),'\0');
                                sprintf(message.recipient, "%s%c", buffer, '\0');
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
                                pthread_exit(NULL);
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
                    pthread_mutex_lock(&mutex_mail);
                    if(mkdir(user_dir_path, 0777) && errno != EEXIST)
                    {
                        perror("error");
                        pthread_exit(NULL);
                    }

                    uuid_t bin_uuid;
                    uuid_generate_random(bin_uuid); // unparse bin_uid to get a usable 36-character string
                    char *uuid = (char *) malloc(sizeof(char) * UUID_SIZE); // allocate memory
                    uuid_unparse_lower(bin_uuid, uuid); // produces a uuid with lower-case letters

                    pthread_mutex_unlock(&mutex_mail);
                    strcat(user_dir_path, "/");
                    snprintf(buffer, BUF, "%s", uuid);

                    strncat(user_dir_path, buffer, strlen(buffer));

                    free(uuid); // free allocated memory

                    pthread_mutex_lock(&mutex_mail);
                    if((file = fopen(user_dir_path, "w")) == NULL)
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
                                pthread_exit(NULL);
                            }
                        }
                        else
                        {
                            send_ok(new_socket);
                        }
                    }
                    pthread_mutex_unlock(&mutex_mail);
                }
            }
            else if((strncmp("list\n", to_lower(buffer), 5) == 0 ||
                     strncmp("list\r", to_lower(buffer), 5) == 0) && logged_in)
            {
                send_ok(new_socket); // befehl bestätigen

                user_dir_path = get_user_dir_path(mail_dir_path, logged_in_user);
                pthread_mutex_lock(&mutex_mail);
                if((dir = opendir(user_dir_path)) == NULL)
                {
                    send_err(new_socket);
                }
                else
                {

                    char *subjects = NULL;
                    if((subjects = list_messages(dir, user_dir_path)) != NULL)
                    {
                        if(writen(new_socket, subjects, strlen(subjects)) < 0)
                        {
                            perror("send error");
                            pthread_exit(NULL);
                        }
                    }
                    else
                    {
                        send_err(new_socket);
                    }
                }
                pthread_mutex_unlock(&mutex_mail);
            }
            else if((strncmp("read\n", to_lower(buffer), 5) == 0 || (strncmp("read\r", to_lower(buffer), 5) == 0)) &&
                    logged_in)
            {
                send_ok(new_socket); // befehl bestätigen

                user_dir_path = get_user_dir_path(mail_dir_path, logged_in_user);

                pthread_mutex_lock(&mutex_mail);
                if((dir = opendir(del_new_line(user_dir_path))) == NULL)
                {
                    send_err(new_socket);
                }
                else
                {
                    int message_number_is_valid = 0;
                    int message_number = 1;

                    if(get_mail_count(user_dir_path) == 0)
                    {
                        send_err(new_socket);
                    }
                    else
                    {
                        send_ok(new_socket); // bereit für nummer-eingabe
                        do
                        {
                            if((check_receive(readline(new_socket, buffer, BUF))))
                            {
                                break;
                            }
                            message_number = (int) strtol(buffer, NULL, 10);
                            if((message_number_is_valid =
                                        message_number > get_mail_count(user_dir_path) || message_number <= 0))
                            {
                                send_err(new_socket);
                            }
                        }
                        while(message_number_is_valid != 0);
                        send_ok(new_socket); // nummer gültig

                        char *message = NULL;
                        if((message = read_message(dir, user_dir_path, message_number)) != NULL)
                        {
                            if(writen(new_socket, message, strlen(message)) < 0)
                            {
                                perror("send error");
                                pthread_exit(NULL);
                            }
                        }
                        else
                        {
                            send_err(new_socket);
                        }
                    }
                }
                pthread_mutex_unlock(&mutex_mail);
            }
            else if((strncmp("del\n", to_lower(buffer), 4) == 0 || strncmp("del\r", to_lower(buffer), 4) == 0) &&
                    logged_in)
            {
                send_ok(new_socket);
                user_dir_path = get_user_dir_path(mail_dir_path, logged_in_user);

                pthread_mutex_lock(&mutex_mail);
                if((dir = opendir(del_new_line(user_dir_path))) == NULL)
                {
                    send_err(new_socket);
                }
                else
                {

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
                pthread_mutex_unlock(&mutex_mail);
            }
        }
    }
    while((strncmp("quit\n", to_lower(buffer), 5) != 0 || strncmp("quit\r", to_lower(buffer), 5) != 0) && !size);
    close(new_socket);
    pthread_exit(NULL);
}
#include "server_functions.h"

int main(int argc, char *argv[])
{
    (void) signal(SIGHUP, sig_handler);
    (void) signal(SIGQUIT, sig_handler);
    (void) signal(SIGINT, sig_handler);

    if(argc != 3)
    {
        print_usage();
    }

    pthread_t tid;
    int new_socket = 0;
    char mail_dir_path[PATH_MAX];
    snprintf(mail_dir_path, strlen(argv[2]) + 1, "%s", argv[2]);
    // strncpy(mail_dir_path, argv[2], strlen(argv[2]));
    int value = 1;

    // create mail spool directory
    if(mkdir(mail_dir_path, 0777) && errno != EEXIST) // do nothing if directory already exists
    {
        perror("mkdir error: ");// error while creating a directory
        exit(EXIT_FAILURE);
    }

    //ip blacklist dir
    if(mkdir("IP_Blacklist", 0777) && errno != EEXIST) // do nothing if directory already exists
    {
        perror("mkdir error: ");// error while creating a directory
        exit(EXIT_FAILURE);
    }

    socklen_t address_length;
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

    // pthread_mutex_init
    pthread_mutex_init(&mutex_mail, NULL);
    pthread_mutex_init(&mutex_ip, NULL);
    while(!0)
    {
        new_socket = accept(create_socket, (struct sockaddr *) &client_address, &address_length);

        if(new_socket > 0)
        {
            fprintf(stdout, "Client connected from %s:%d...\n", inet_ntoa(client_address.sin_addr),
                    ntohs(client_address.sin_port));

            struct thread_parameter parameter;
            parameter.socket = new_socket;
            parameter.client_address = client_address;
            parameter.mail_dir_path = mail_dir_path;

            if(pthread_create(&tid, NULL, server_function, &parameter) != 0)
            {
                fprintf(stdout, "Failed to create thread\n");
            }
        }
    }
}
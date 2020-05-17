#include "httpserver.h"

// void handle_request (struct fooObject* foo);

/*
    \brief 1. Want to read in the HTTP message/ data coming in from socket
    \param client_sockd - socket file descriptor
    \param message - object we want to 'fill in' as we read in the HTTP message
*/
void read_http_request(ssize_t client_sockd, struct httpObject* message) {
    printf("----Reading Request----\n");
    message->content_length = message->status_code = 0;

    /*
     * Start constructing HTTP request based off data from socket
     */

    ssize_t bytes = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
    message->buffer[bytes] = 0;

    char buff[bytes];
    memcpy(buff, message->buffer, bytes);

    // printf("Printing Buffer...\n%s", buff);

    char* token;
    const char* whitespace = " \r\t\n\v\f";
    token = strtok(buff, whitespace);
    strcpy(message->method, token); // Parse Method

    token = strtok(NULL, whitespace);
    memcpy(message->filename, token+1, strlen(token)); // Parse Filename 

    token = strtok(NULL, whitespace);
    strcpy(message->httpversion, token); // Parse HTTPversion

    // Find Content-Length if it exists
    const char contentlen[20] = "Content-Length:";
    char* ret;
    ret = strstr((char*)message->buffer, contentlen);
    if (ret) {
        token = strtok(ret, whitespace);
        token = strtok(NULL, whitespace);
        message->content_length = atoi(token);
    }

    printf("method: %s\n", message->method);
    printf("file: %s\n", message->filename);
    printf("httpversion: %s\n", message->httpversion);
    printf("content_len: %ld\n", message->content_length);
}

/*
    \brief 2. Want to process the message we just recieved
*/
void process_request(ssize_t client_sockd, struct httpObject* message, struct healthObject* health) {
    printf("----Processing Request----\n");

    if (strcmp(message->httpversion, "HTTP/1.1") != 0) {
        printf("Invalid HTTP version\n");
        message->status_code = 400;
        message->content_length = 0;
        return;
    }

    if (!filenamecheck(message->filename)) {
        printf("Invalid file name: %s\n", message->filename);
        message->status_code = 400;
        message->content_length = 0;
        return;
    }

    if (strcmp(message->filename, "healthcheck") == 0) {
        message->status_code = 200;
        // Data to be returned is a string ("%d\n%d", errors, entries)
        message->content_length = nDigits(health->errors) + 1 + nDigits(health->entries);
        return;
    }

    // Open file and extract file stats
    struct stat info;
    int stat_ret = stat(message->filename, &info);

    // GET will send file data in construct_http_response()
    if (strcmp(message->method, "HEAD") == 0 || strcmp(message->method, "GET") == 0) {
        // Check file exists and permissions
        // and set status_code, content_len accordingly
        if (stat_ret == -1){
            printf("file error: %s: does not exist\n", message->filename);
            message->status_code = 404;
            message->content_length = 0;

        } else if ((info.st_mode & S_IRUSR) != S_IRUSR){
            printf("file error: %s: bad permissions\n", message->filename);
            message->status_code = 403;
            message->content_length = 0;

        } else {
            message->status_code = 200;
            message->content_length = info.st_size;
        }

    } else if (strcmp(message->method, "PUT") == 0) {

        // Check if file exists and permissions correct
        // and set status_code, content_len accordingly
        if (stat_ret == -1) {
            message->status_code = 201;
            printf("creating file: %s\n", message->filename);
        } else if ((info.st_mode & S_IWUSR) != S_IWUSR) {
            printf("file error: %s: Does not have write permission\n", message->filename);
            message->status_code = 403;
            message->content_length = 0;
            return;
        } else {
            message->status_code = 200;
        }

        char double_crlf[5] = "\r\n\r\n";
        char* body;
        ssize_t bytes = BUFFER_SIZE, total_bytes = 0;
        mode_t permissions = 0666; // Sets file permissions to u=rw on O_CREAT
        
        int fd = open(message->filename, O_RDWR | O_TRUNC | O_CREAT, permissions);

        // Write bytes that are in same message as header
        body = strstr((char*) message->buffer, double_crlf);
        if (body) {
            total_bytes += write(fd, body+4, message->content_length);
        }

        // Write rest of bytes received
        while (total_bytes < message->content_length) {
            bytes = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
            write(fd, message->buffer, bytes);
            total_bytes += bytes;
        }
        close(fd);
        message->content_length = 0;

    } else {
        printf("Method not implemented");   
        message->status_code = 400;
        message->content_length = 0;
    }
}

/*
    \brief 3. Construct some response based on the HTTP request you recieved
*/
void construct_http_response(ssize_t client_sockd, struct httpObject* message, struct healthObject* health) {
    printf("----Constructing Response----\n");

    char* status_message = "";
    switch(message->status_code) {
        case 200 :
            status_message = "OK";
            break;
        case 201 :
            status_message = "Created";
            break;
        case 400 :
            status_message = "Bad Request";
            break;
        case 403 :
            status_message = "Forbidden";
            break;
        case 404 :
            status_message = "File not found";
            break;
        case 500 :
            status_message = "Internal Server Error";
            break;
        default :
            status_message = "No valid status code. You fucked up";
            break;
    }

    char reply[BUFFER_SIZE] = "";

    snprintf(reply, BUFFER_SIZE, "%s %d %s\r\nContent-Length: %ld\r\n\r\n",
        message->httpversion, message->status_code, status_message, message->content_length);
    send(client_sockd, reply, strlen(reply), 0);
    memset(reply, 0, strlen(reply));
    if (strcmp(message->filename, "healthcheck") == 0) {
        snprintf(reply, message->content_length + 1, "%d\n%d", health->errors, health->entries);
        send(client_sockd, reply, strlen(reply), 0);
        // printf("strlen:%ld\n", strlen(reply));
        // printf("reply:%s\n", reply);
    }   
    // Send file data if valid GET request and not a healthcheck
    if (message->status_code == 200 
      && strcmp(message->method, "GET") == 0 
      && strcmp(message->filename, "healthcheck") != 0) {

        int fd = open(message->filename, O_RDONLY);
        // Write file data
        ssize_t size = BUFFER_SIZE;
        while (size != 0) {
        printf("closefile");
            size = read(fd, message->buffer, BUFFER_SIZE);
            send(client_sockd, message->buffer, size, 0);
        }
        close(fd);
    }
    write_log(message);
}

int main(int argc, char** argv) {

    if (argc < 2 || argc > 6) {
        usage();
    }

    // Parse options 
    int opt, numthreads = 4, port;
    char* logfile = NULL;
    while ((opt = getopt(argc, argv, "N:l:")) != -1) {
        switch (opt) {
        case 'N':
            numthreads = atoi(optarg);
            break;
        case 'l':
            logfile = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s [-lN] [file...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // If no port specified
    if (argc == optind) {
        usage();
    }

    port = atoi(argv[optind]);
    // printf("numthread:%d\nlogfile:%s\n", numthreads, logfile);

    /*
        Create sockaddr_in with server information
    */
    // char* port = argv[1];
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t addrlen = sizeof(server_addr);

    /*
        Create server socket
    */
    int server_sockd = socket(AF_INET, SOCK_STREAM, 0);

    // Need to check if server_sockd < 0, meaning an error
    if (server_sockd < 0) {
        perror("socket");
    }

    /*
        Configure server socket
    */
    int enable = 1;

    /*
        This allows you to avoid: 'Bind: Address Already in Use' error
    */
    int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    /*
        Bind server address to socket that is open
    */
    ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);

    /*
        Listen for incoming connections
    */
    ret = listen(server_sockd, 5); // 5 should be enough, if not use SOMAXCONN

    if (ret < 0) {
        return EXIT_FAILURE;
    }

    /*
        Connecting with a client
    */
    struct sockaddr client_addr;
    socklen_t client_addrlen;

    struct httpObject message;
    struct healthObject health = {0,0};
    // struct fooObject foo;
    
    if( logfile ){
        int log_fd = open(logfile, O_RDWR | O_TRUNC | O_CREAT, (mode_t)0666);
        message.log_fd = log_fd;
    }
    // foo.logfile = logfile;

    // pthread_t thread1;
    // int* pclient = malloc(sizeof(int));
    
    while (true) {
        printf("[+] server is waiting...\n");

        /*
         * 1. Accept Connection
         */
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        // foo.client_sockd = client_sockd;
        // struct fooObject *pfoo = malloc(sizeof(struct fooObject));
        // pthread_create(&thread1, NULL, handle_request, pfoo);
        // Remember errors happen
        // handle_request(&foo);

        /*
         * 2. Read HTTP Message
         */
        read_http_request(client_sockd, &message);

        /*
         * 3. Process Request
         */
        process_request(client_sockd, &message, &health);

        /*
         * 4. Construct Response
         */
        construct_http_response(client_sockd, &message, &health);

        /*
         * 5. Send Response
         */
        printf("Response Sent\n\n");

        health.entries++;
        if (message.status_code > 201) {
            health.errors++;
        }
    }
    return EXIT_SUCCESS;
}

// void * handle_request (struct fooObject* foo){
//     struct httpObject message;

//     struct healthObject health = {0,0};
    
//     mode_t log_permissions = 0666;
//     if( foo->logfile ){
//         printf("logfile present\n");
//         int log_fd = open(foo->logfile, O_RDWR | O_TRUNC | O_CREAT, log_permissions);
//         message.log_fd = log_fd;
//     } else {
//         printf("logfile NOT present\n");
//         message.log_fd = -1;
//     }

//     /*
//          * 2. Read HTTP Message
//          */
//         read_http_request(foo->client_sockd, &message);

//         /*
//          * 3. Process Request
//          */
//         process_request(foo->client_sockd, &message, &health);

//         /*
//          * 4. Construct Response
//          */
//         construct_http_response(foo->client_sockd, &message, &health);

//         /*
//          * 5. Send Response
//          */
//         printf("Response Sent\n\n");
// }
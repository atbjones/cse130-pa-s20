#include "httpserver.h"

#define BUFFER_SIZE 4096
extern int errno;

struct httpObject {
    /*
        Create some object 'struct' to keep track of all
        the components related to a HTTP message
        NOTE: There may be more member variables you would want to add
    */
    char method[5];         // PUT, HEAD, GET
    char filename[28];      // what is the file we are worried about
    char httpversion[9];    // HTTP/1.1
    ssize_t content_length; // example: 13
    int status_code;
    uint8_t buffer[BUFFER_SIZE];
};

/*
    \brief 1. Want to read in the HTTP message/ data coming in from socket
    \param client_sockd - socket file descriptor
    \param message - object we want to 'fill in' as we read in the HTTP message
*/
void read_http_response(ssize_t client_sockd, struct httpObject* message) {
    printf("This function will take care of reading message\n");

    /*
     * Start constructing HTTP request based off data from socket
     */

    char buf[BUFFER_SIZE];
    // ssize_t size = BUFFER_SIZE;

    read(client_sockd, buf, BUFFER_SIZE);

    char* token;
    const char* whitespace = " \r\t\n";
    token = strtok(buf, whitespace);
    strcpy(message->method, token); // Parse Method

    token = strtok(NULL, whitespace);
    memcpy(message->filename, token+1, sizeof(&token)); // Parse Filename 

    token = strtok(NULL, whitespace);
    strcpy(message->httpversion, token); // Parse HTTPversion

    printf("method: %s\n", message->method);
    printf("file: %s\n", message->filename);
    printf("httpversion: %s\n", message->httpversion);
    // printf("content_len: %ld\n", message->content_length);
    // printf("status_code: %d\n", message->status_code);
    // printf("size: %ld\n", size);
    // printf("buffer: %s\n", message->buffer);
    // message->filename[1] = 'f';

    return;
}

/*
    \brief 2. Want to process the message we just recieved
*/
void process_request(ssize_t client_sockd, struct httpObject* message) {
    printf("Processing Request\n");

    if (!filenamecheck(message->filename)) {
        printf("Invalid file name: %s\n", message->filename);
        message->status_code = 400;
        message->content_length = 0;
        return;
    }

    if (strcmp(message->method, "HEAD") == 0){
        printf("Processing HEAD\n");

        // Open file and extract file size
        struct stat finfo;
        int fd = open(message->filename, O_RDONLY);
        if (fd != -1) {
            fstat(fd, &finfo);

            message->content_length = finfo.st_size;
            message->status_code = 200;
        } else {
            fprintf(stderr, "file error: %s: %s\n", message->filename, strerror(errno));

            message->content_length = 0;
            if (errno == EACCES){ // Errno is 13/EACCESS if file does not have r permission
                message->status_code = 403;
            } else if (errno == 2) { // Error code is 2 if file does not exist
                message->status_code = 404;
            }
        }
        close(fd);
    } else if (strcmp(message->method, "GET") == 0) {
        printf("Processing GET\n");

        struct stat finfo;
        int fd = open(message->filename, O_RDONLY);
        if (fd != -1) {
            fstat(fd, &finfo);

            message->content_length = finfo.st_size;
            message->status_code = 200;
        } else {
            fprintf(stderr, "file error: %s: %s\n", message->filename, strerror(errno));

            message->content_length = 0;
            if (errno == EACCES){
                message->status_code = 403;
            } else if (errno == 2){
                message->status_code = 404;
            }
        }
        close(fd);
    } else if (strcmp(message->method, "PUT") == 0) {
        printf("Processing PUT\n");

        if (file_exists(message->filename)) {
            message->status_code = 200;
        } else {
            message->status_code = 201;
        }

        char buf[BUFFER_SIZE];
        ssize_t size = BUFFER_SIZE;

        mode_t permissions = 0666; // Sets file permissions to u=rw on O_CREAT
        int fd = open(message->filename, O_RDWR | O_TRUNC | O_CREAT, permissions);
        if (fd != -1) {
            size = read(client_sockd, buf, BUFFER_SIZE);
            write(fd, buf, size);

            message->content_length = 0;
        } else {
            fprintf(stderr, "file error: %s: %s\n", message->filename, strerror(errno));

            message->content_length = 0;
            message->status_code = 403;
        }
        close(fd);

    } else {
        printf("Method not implemented");
        
        message->content_length = 0;
        message->status_code = 500;
    }
}

/*
    \brief 3. Construct some response based on the HTTP request you recieved
*/
void construct_http_response(ssize_t client_sockd, struct httpObject* message) {
    printf("Constructing Response\n");

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
    // printf("length:%ld\n", message->content_length);
    printf("code:%d\n", message->status_code);

    if (strcmp(message->method, "HEAD") == 0){
        printf("Response HEAD\n");

        snprintf(reply, BUFFER_SIZE-1, "%s %d %s\r\nContent-Length: %ld\r\n\r\n",
        message->httpversion, message->status_code, status_message, message->content_length);
        
        send(client_sockd, reply, strlen(reply), 0);

    } else if (strcmp(message->method, "GET") == 0) {
        printf("Response GET\n");

        snprintf(reply, BUFFER_SIZE-1, "%s %d %s\r\nContent-Length: %ld\r\n\r\n",
        message->httpversion, message->status_code, status_message, message->content_length);

        send(client_sockd, reply, strlen(reply), 0);

        if (message->status_code == 200) {
            // Open specified file
            int fd = open(message->filename, O_RDONLY);

            // Write file data
            ssize_t size = BUFFER_SIZE;
            while (size != 0) {
                size = read(fd, message->buffer, BUFFER_SIZE);
                write(client_sockd, message->buffer, size);
            }

            close(fd);
        }
        
    } else if (strcmp(message->method, "PUT") == 0) {
        printf("Response PUT\n");

        snprintf(reply, BUFFER_SIZE-1, "%s %d %s\r\nContent-Length: %ld\r\n\r\n",
        message->httpversion, message->status_code, status_message, message->content_length);
        
        send(client_sockd, reply, strlen(reply), 0);
    }

    // int fd = open("foo", O_RDONLY);
    // if (fd != -1) {
    //     readfile(fd);
    // } else {
    //     fprintf(stderr, "server error: %s: %s\n", "foo", strerror(errno));
    // }

    return;
}

void usage(){
    printf("usage: ./httpserver <port>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    if (argc != 2 || strcmp(argv[1], "1025") < 0) usage();

    /*
        Create sockaddr_in with server information
    */
    char* port = argv[1];
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
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

    while (true) {
        printf("[+] server is waiting...\n");

        /*
         * 1. Accept Connection
         */
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        // Remember errors happen

        /*
         * 2. Read HTTP Message
         */
        read_http_response(client_sockd, &message);

        /*
         * 3. Process Request
         */
        process_request(client_sockd, &message);

        /*
         * 4. Construct Response
         */
        construct_http_response(client_sockd, &message);

        /*
         * 5. Send Response
         */
        printf("Response Sent\n\n");

        /*
         * Sample Example which wrote to STDOUT once.
         *
        uint8_t buff[BUFFER_SIZE + 1];
        ssize_t bytes = recv(client_sockd, buff, BUFFER_SIZE, 0);
        buff[bytes] = 0; // null terminate
        printf("[+] received %ld bytes from client\n[+] response: \n", bytes);
        write(STDOUT_FILENO, buff, bytes);
        */
    }

    return EXIT_SUCCESS;
}

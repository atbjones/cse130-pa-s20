#include "httpserver.h"

// #define BUFFER_SIZE 4096
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
void read_http_request(ssize_t client_sockd, struct httpObject* message) {
    printf("Reading Request\n");

    /*
     * Start constructing HTTP request based off data from socket
     */

    ssize_t bytes = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
    message->buffer[bytes] = 0;

    char buff[bytes];
    memcpy(buff, message->buffer, bytes);
    // buff[bytes] = '\0';

    // printf("Printing Buffer...\n%s", buff);
    // for (ssize_t i = 0; i < bytes; i++) {
    //     printf("%c", buff[i]);
    // }

    char* token;
    const char* whitespace = " \r\t\n\v\f";
    token = strtok(buff, whitespace);
    strcpy(message->method, token); // Parse Method

    token = strtok(NULL, whitespace);
    // printf("sizeof:%ld\n", strlen(token));
    memcpy(message->filename, token+1, strlen(token)); // Parse Filename 

    token = strtok(NULL, whitespace);
    strcpy(message->httpversion, token); // Parse HTTPversion

    // Find Content-Length if it exists
    const char temp[20] = "Content-Length:";
    char* ret;

    ret = strstr(message->buffer, temp);
    if (ret) {
        token = strtok(ret, whitespace);
        token = strtok(NULL, whitespace);
        message->content_length = atoi(token);
    }

    // while (token != NULL) {
    //     token = strtok(NULL, whitespace);
    //     printf("token: %s-\n", token);
    // }

    printf("method: %s\n", message->method);
    printf("file: %s\n", message->filename);
    printf("httpversion: %s\n", message->httpversion);
    printf("content_len: %ld\n", message->content_length);
}

/*
    \brief 2. Want to process the message we just recieved
*/
void process_request(ssize_t client_sockd, struct httpObject* message) {
    printf("Processing Request\n");

    if (strcmp(message->httpversion, "HTTP/1.1") != 0) {
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

    // Open file and extract file stats
    struct stat info;
    int exists = stat(message->filename, &info);

    if (strcmp(message->method, "HEAD") == 0 || strcmp(message->method, "GET") == 0) {

        // printf("Permission:%3o\n", info.st_mode);

        if (exists == -1){
            printf("ERROR: File does not exist\n");
            message->status_code = 404;
            message->content_length = 0;

        } else if ((info.st_mode & S_IRUSR) != S_IRUSR){
            printf("ERROR: Does not have read Permission\n");
            message->status_code = 403;
            message->content_length = 0;

        } else {
            message->status_code = 200;
            message->content_length = info.st_size;
        }

    } else if (strcmp(message->method, "PUT") == 0) {

        // Assign status code here based on if the file already exists
        if (exists == -1) {
            message->status_code = 201;
        } else if ((info.st_mode & S_IWUSR) != S_IWUSR) {
            printf("ERROR: Does not have write permission\n");
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
        if (fd != -1) {

            // Write bytes that are in same message as header
            body = strstr(message->buffer, double_crlf);
            if (body) {
                total_bytes += write(fd, body+4, message->content_length);
            }
            printf("body:%s--\n", body);

            printf("here:%ld\n", total_bytes);

            // Write rest of bytes received //total_bytes < message->content_length
            while (total_bytes < message->content_length) {
                bytes = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
                total_bytes += bytes;
                write(fd, message->buffer, bytes);
                // printf("inwhile:%ld\n", total_bytes);
            }
        } else {
            fprintf(stderr, "file error: %s: %s\n", message->filename, strerror(errno));

            message->status_code = 403;
        }
        close(fd);

        message->content_length = 0;

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
    printf("code:%d\n", message->status_code);

    snprintf(reply, BUFFER_SIZE-1, "%s %d %s\r\nContent-Length: %ld\r\n\r\n",
    message->httpversion, message->status_code, status_message, message->content_length);
        
    send(client_sockd, reply, strlen(reply), 0);

    if (message->status_code == 200 && strcmp(message->method, "GET") == 0) {
        // Open specified file
        int fd = open(message->filename, O_RDONLY);

        // Write file data
        ssize_t size = BUFFER_SIZE;
        while (size != 0) {
            size = read(fd, message->buffer, BUFFER_SIZE);
            send(client_sockd, message->buffer, size, 0);
        }

        close(fd);
    }

    return;
}

void usage(){
    printf("usage: ./httpserver <port>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    if (argc != 2 || strcmp(argv[1], "1024") <= 0) usage();

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
        read_http_request(client_sockd, &message);

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

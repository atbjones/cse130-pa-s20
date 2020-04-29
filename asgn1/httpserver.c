#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h> // write
#include <string.h> // memset
#include <stdlib.h> // atoi
#include <stdbool.h> // true, false

#define BUFFER_SIZE 4096

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

    return;
}

/*
    \brief 2. Want to process the message we just recieved
*/
void process_request() {
    printf("Processing Request\n");

    return;
}

/*
    \brief 3. Construct some response based on the HTTP request you recieved
*/
void construct_http_response() {
    printf("Constructing Response\n");

    return;
}


int main(int argc, char** argv) {
    /*
        Create sockaddr_in with server information
    */
    char* port = "8080";
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
        process_request();

        /*
         * 4. Construct Response
         */
        construct_http_response();

        /*
         * 5. Send Response
         */
        printf("Response Sent\n");

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

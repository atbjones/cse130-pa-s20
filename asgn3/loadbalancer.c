#include<err.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>

/*
 * client_connect takes a port number and establishes a connection as a client.
 * connectport: port number of server to connect to
 * returns: valid socket if successful, -1 otherwise
 */
int client_connect(uint16_t connectport) {
    int connfd;
    struct sockaddr_in servaddr;

    connfd=socket(AF_INET,SOCK_STREAM,0);
    if (connfd < 0)
        return -1;
    memset(&servaddr, 0, sizeof servaddr);

    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(connectport);

    /* For this assignment the IP address can be fixed */
    inet_pton(AF_INET,"127.0.0.1",&(servaddr.sin_addr));

    if(connect(connfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0)
        return -1;
    return connfd;
}

/*
 * server_listen takes a port number and creates a socket to listen on 
 * that port.
 * port: the port number to receive connections
 * returns: valid socket if successful, -1 otherwise
 */
int server_listen(int port) {
    int listenfd;
    int enable = 1;
    struct sockaddr_in servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
        return -1;
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
        return -1;
    if (bind(listenfd, (struct sockaddr*) &servaddr, sizeof servaddr) < 0)
        return -1;
    if (listen(listenfd, 500) < 0)
        return -1;
    return listenfd;
}

/*
 * bridge_connections send up to 100 bytes from fromfd to tofd
 * fromfd, tofd: valid sockets
 * returns: number of bytes sent, 0 if connection closed, -1 on error
 */
int bridge_connections(int fromfd, int tofd) {
    char recvline[100];
    int n = recv(fromfd, recvline, 100, 0);
    if (n < 0) {
        printf("connection error receiving\n");
        return -1;
    } else if (n == 0) {
        printf("receiving connection ended\n");
        return 0;
    }
    recvline[n] = '\0';
    printf("%s", recvline);
    sleep(1);
    n = send(tofd, recvline, n, 0);
    if (n < 0) {
        printf("connection error sending\n");
        return -1;
    } else if (n == 0) {
        printf("sending connection ended\n");
        return 0;
    }
    return n;
}

/*
 * bridge_loop forwards all messages between both sockets until the connection
 * is interrupted. It also prints a message if both channels are idle.
 * sockfd1, sockfd2: valid sockets
 */
void bridge_loop(int sockfd1, int sockfd2) {
    fd_set set;
    struct timeval timeout;

    int fromfd, tofd;
    while(1) {
        // set for select usage must be initialized before each select call
        // set manages which file descriptors are being watched
        FD_ZERO (&set);
        FD_SET (sockfd1, &set);
        FD_SET (sockfd2, &set);

        // same for timeout
        // max time waiting, 5 seconds, 0 microseconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // select return the number of file descriptors ready for reading in set
        switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
            case -1:
                printf("error during select, exiting\n");
                return;
            case 0:
                printf("both channels are idle, waiting again\n");
                continue;
            default:
                if (FD_ISSET(sockfd1, &set)) {
                    fromfd = sockfd1;
                    tofd = sockfd2;
                } else if (FD_ISSET(sockfd2, &set)) {
                    fromfd = sockfd2;
                    tofd = sockfd1;
                } else {
                    printf("this should be unreachable\n");
                    return;
                }
        }
        if (bridge_connections(fromfd, tofd) <= 0)
            return;
    }
}

int main(int argc,char **argv) {
    int connfd, listenfd, acceptfd;
    uint16_t connectport, listenport;

    if (argc < 3) {
        printf("missing arguments: usage %s port_to_connect port_to_listen", argv[0]);
        return 1;
    }

    int opt, num_paras = 4, num_req = 5;
    while ((opt = getopt(argc, argv, "N:R:")) != -1) {
        switch (opt) {
        case 'N':
            num_paras = atoi(optarg);
            if (num_paras <= 0){
                fprintf(stderr, "Usage: %s [-RN] [port_to_connect] [port_to_listen]\n", argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'R':
            num_req = atoi(optarg);
            if (num_req <= 0){
                fprintf(stderr, "Usage: %s [-RN] [port_to_connect] [port_to_listen]\n", argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Usage: %s [-RN] [port_to_connect] [port_to_listen]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // If no connectport or listenport specified
    if (optind + 2 != argc) {
        fprintf(stderr, "Usage: %s [-RN] [port_to_connect] [port_to_listen]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    connectport = atoi(argv[optind]);
    listenport = atoi(argv[optind+1]);


    // Remember to validate return values
    // You can fail tests for not validating
    connectport = atoi(argv[1]);
    listenport = atoi(argv[2]);
    if ((connfd = client_connect(connectport)) < 0)
        err(1, "failed connecting");
    if ((listenfd = server_listen(listenport)) < 0)
        err(1, "failed listening");
    if ((acceptfd = accept(listenfd, NULL, NULL)) < 0)
        err(1, "failed accepting");

    // This is a sample on how to bridge connections.
    // Modify as needed.
    bridge_loop(acceptfd, connfd);
}
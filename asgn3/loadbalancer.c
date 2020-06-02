#include "loadbalancer.h"

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
    char buff[BUFFER_SIZE];
    int n = BUFFER_SIZE;
    while (n == BUFFER_SIZE)  {
        // printf("recv\n");
        n = recv(fromfd, buff, BUFFER_SIZE, 0);
        if (n < 0) {
            printf("connection error receiving\n");
            return -1;
        } else if (n == 0) {
            printf("receiving connection ended\n");
            return 0;
        }
        buff[n] = '\0';
        printf("[+]Buffer\n****************************\n%s\n****************************\n", buff);
        // printf("send\n");
        n = send(tofd, buff, n, 0);
        if (n < 0) {
            printf("connection error sending\n");
            return -1;
        } else if (n == 0) {
            printf("sending connection ended\n");
            return 0;
        }
        // printf("done\n");
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

/*
 * health_check_probe sends a healthcheck request to the specified server
 * and updates the object. Will set both health fields to -1 if no response
 */
void health_check_probe(struct serverObject * server){
    int connfd;
    char buff[100];
    // printf("here1\n");
    if ((connfd = client_connect(server->port)) < 0)
        err(1, "failed connecting");
    int n = send(connfd, health_check_request, 100, 0);
    if (n < 0) {
        printf("connection error sending\n");
        server->errors = -1;
        server->entries = -1;
    } else if (n == 0) {
        printf("in healthcheckprobe... idk man.\n");
        return;
    }
    n = recv(connfd, buff, 100, 0);
    // printf("here2%s\n",buff);
    char * ret = strstr(buff, "\r\n\r\n");

// printf("here3\n");
    char * token = strtok(ret, whitespace);
    // printf("here3.5%s\n",token);
    if (token != NULL) {
        server->errors = atoi(token);

        token = strtok(NULL, whitespace);
        if (token != NULL) {
            server->entries = atoi(token);
        }
    } else {
        fprintf(stderr, "Weird error, no body recvd");
        server->errors = -1;
        server->entries = -1;
    }

// printf("here4\n");
}

/*
 * chooseserver will select the next server to send a request to the
 * server that has had the least requests sent to it or to the server
 * with the least errors in case of a tie. In case of a double tie,
 * it will choose the smaller serverid
 */
// int choose_server(struct serverObject servers[]){
//     int min = 0;
//     size_t n = sizeof(servers)/sizeof(servers[0]);
//     for (size_t i = 0; i < n; i++){
//         printf("serverid:%d\n", servers[i].id);
//         if (servers[i].requests < servers[min].requests)
//             min = i;
//     }
//     return min;
// }

int main(int argc,char **argv) {
    int connfd, listenfd, acceptfd;
    uint16_t connectport, listenport;

    if (argc < 3) {
        printf("missing arguments: usage %s port_to_connect port_to_listen", argv[0]);
        return 1;
    }

    // Option parsing
    int opt, num_paras = 4, num_req = 5;
    while ((opt = getopt(argc, argv, "N:R:")) != -1) {
        switch (opt) {
        case 'N':
            if ((num_paras = atoi(optarg)) <= 0){
                usage();
            }
            break;
        case 'R':
            if ((num_req = atoi(optarg)) <= 0){
                usage();
            }
            break;
        default:
            usage();
        }
    }

    // If no server ports or listening ports specified
    if (optind + 2 > argc) {
        usage();
    }

    // Set listening port to first port
    listenport = atoi(argv[optind]);
    if (listenport <=0) {
        usage();
    }

    // Initialize server objects     
    int num_servers = argc-optind-1;
    struct serverObject servers[num_servers];
    for (int i = 0; i < num_servers; i++){
        servers[i].id = i;
        servers[i].port = atoi(argv[optind+1+i]);
        printf("prot:%d\n", servers[i].port);

        // Send a health check probe to initialize server health
        health_check_probe(&servers[i]);

        // Connect
        // if ((connfd = client_connect(servers[i].port)) < 0)
        //     err(1, "failed connecting");
    }

    // Remember to validate return values
    // You can fail tests for not validating
    if ((listenfd = server_listen(listenport)) < 0)
        err(1, "failed listening");

    int requests = 0, next = 0;
    while (true) {

        printf("[+] load balancer waiting...\n");
        if ((acceptfd = accept(listenfd, NULL, NULL)) < 0)
            err(1, "failed accepting");

        requests++;

        // Perform routine healthcheck
        if (requests % num_req == 0) {
            for (int i = 0; i < num_servers; i++) {
                printf("[+] checking health");
                health_check_probe(&servers[i]);
            }            
        }

        // Choose next server to send request to
        next = 0;
        for (int i = 0; i < num_servers; i++){
            if (servers[i].entries < servers[next].entries && servers[i].entries != -1)
                next = i;
        }
        printf("[+] next:%d\n", next);

        if ((connfd = client_connect(servers[next].port)) < 0)
            err(1, "failed connecting");

        // This is a sample on how to bridge connections.
        // Modify as needed.
        bridge_loop(acceptfd, connfd);

        // Increment server requests count
        // TODO: CHECK IF ERROR OR ENTRY AND INCREMENT
        servers[next].entries += 1;
    }

    return EXIT_SUCCESS;
}
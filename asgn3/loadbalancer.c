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
 * bridge_connections send up to 4096 bytes at a time from fromfd to tofd
 * fromfd, tofd: valid sockets
 * returns: number of bytes sent, 0 if connection closed, -1 on error
 */
int bridge_connections(int fromfd, int tofd) {
    // printf("in b_c\n");
    uint8_t buff[BUFFER_SIZE];
    int n = BUFFER_SIZE;
    while (n == BUFFER_SIZE)  {
        n = recv(fromfd, buff, BUFFER_SIZE, 0);
        printf("recv:%d\n", n);
        if (n < 0) {
            printf("connection error receiving\n");
            return -1;
        } else if (n == 0) {
            printf("receiving connection ended\n");
            // send_500(tofd);
            return 0;
        }
        buff[n] = '\0';
        printf("[+]Buffer\n****************************\n%s\n****************************\n\n", buff);
        // sleep(10);
        n = send(tofd, buff, n, 0);
        printf("send:%d\n", n);
        if (n < 0) {
            printf("connection error sending\n");
            return -1;
        } else if (n == 0) {
            printf("sending connection ended\n");
            return 0;
        }
        printf("done:%d\n", n);
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

void* handle_task(void* pair){
    struct pair* P = (struct pair*)pair;
    bridge_loop(P->fd1, P->fd2);
}

/*
 * health_check_probe sends a healthcheck request to the specified server
 * and updates the object. Will set both health fields to -1 if no response
 */
void health_check_probe(struct serverObject * server){
    int connfd;
    char buff[100];
    // printf("here1\n");
    if ((connfd = client_connect(server->port)) < 0){
        printf("[+] server %d did not respond to a health_check\n", server->port);
        server->alive = false;
        return;
    }
    int n = send(connfd, health_check_request, sizeof(health_check_request), 0);
    if (n < 0) {
        printf("connection error sending\n");
        server->alive = false;
        return;
    } else if (n == 0) {
        printf("in healthcheckprobe... idk man.\n");
        return;
    }

    // Get response and put in buff
    int bytes_recvd = 0;
    n = recv(connfd, buff, 100, 0);
    bytes_recvd = n;
    while (n != 0) {
        n = recv(connfd, buff + n, 100, 0);
        bytes_recvd += n;
    }
    buff[bytes_recvd] = '\0';
    // printf("[+] buff:\n%s\n",buff);
    // char * ret1 = strstr(buff, "Content-Length: ");
    // char * token1 = strtok(ret1, whitespace);
    // token1 = strtok(NULL, whitespace);
    // printf("token:%s\n", token1);
    // cont_len = atoi(token1);

    char * ret = strstr(buff, "\r\n\r\n");

    char * token = strtok(ret, whitespace);
    // printf("here3.5%s\n",token);
    if (token != NULL) {
        server->errors = atoi(token);

        token = strtok(NULL, whitespace);
        // printf("here3.5.5%s\n",token);
        if (token != NULL) {
            server->entries = atoi(token);
        }
    } else {
        fprintf(stderr, "No body recvd. This should not ever happen if server returns correct healthcheck\n");
        server->alive = false;
    }
    // printf("server %d: entries:%d errors:%d\n", server->port, server->entries, server->errors);

    server->alive = true;
}

/*
 * healthcheck_loop will send a healthcheck to each server 
 * every 5 seconds indefeinitly
 */
void * health_check_loop(void * ptr) {
    // struct serverObject servers[2];
    struct health_thread_Object* p_health = (struct health_thread_Object*)ptr;
    struct serverObject* p_servers = (struct serverObject*)p_health->p_servers;
    int* p_num_reqs = p_health->p_num_reqs;
    int num_servers = p_health->num_servers;
    int prev = 0;
    // printf("ptr:%p\n", p_servers);
    while(true) {
        sleep(5);
        if (*p_num_reqs - prev >= 5){
            prev = *p_num_reqs;
            continue;
        }
        for (int i = 0; i < num_servers; i++) {
            // printf("[+] checking health of %d\n", (p_servers+i)->port);
            health_check_probe(p_servers+i);
        }
        prev = *p_num_reqs;
    }
}

/*
 * chooseserver will select the next server to send a request to the
 * server that has had the least requests sent to it or to the server
 * with the least errors in case of a tie. In case of a double tie,
 * it will choose the smaller serverid
 */
int choose_server(struct serverObject servers[], int num_servers){
    int min = 0;
    for (int i = 0; i < num_servers; i++){
        // printf("serverid:%d\n", servers[i].id);
        if (servers[i].entries < servers[min].entries && servers[i].alive){
            min = i;
        } else if (servers[i].entries == servers[min].entries) {
            if (servers[i].errors < servers[min].errors)
            min = i;
        }
    }
    // If min is not alive, all servers are dead.
    if (servers[min].alive){
        return min;
    } else {
        return -1;
    }
}

// ============================================================================
int main(int argc,char **argv) {
    int serverfd, listenfd, clientfd;
    uint16_t listenport;

    if (argc < 3) {
        printf("missing arguments: usage %s port_to_connect port_to_listen", argv[0]);
        return 1;
    }

    // Option parsing
    int opt, num_paras = 4, R = 5;
    while ((opt = getopt(argc, argv, "N:R:")) != -1) {
        switch (opt) {
        case 'N':
            if ((num_paras = atoi(optarg)) <= 0){
                usage();
            }
            break;
        case 'R':
            if ((R = atoi(optarg)) <= 0){
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

    int num_requests = 0;

    // Initialize server objects     
    int num_servers = argc-optind-1;
    struct serverObject servers[num_servers];
    for (int i = 0; i < num_servers; i++){
        servers[i].id = i;
        servers[i].port = atoi(argv[optind+1+i]);

        // Send a health check probe to initialize server health
        health_check_probe(&servers[i]);

        // Connect
        // if ((connfd = client_connect(servers[i].port)) < 0)
        //     err(1, "failed connecting");
    }


    struct health_thread_Object* p_health_pkg = malloc(sizeof(struct health_thread_Object));
    p_health_pkg->num_servers = num_servers;
    p_health_pkg->p_servers = servers;
    p_health_pkg->p_num_reqs = &num_requests;
    // p_health_pkg->auto_health = false;

    // Create healthcheck thread
    pthread_t health_thread;
    pthread_create(&health_thread, NULL, health_check_loop, p_health_pkg);

    // Create N threads
    pthread_t thread_id;
    struct pair pair;
    struct pair * p_pair = &pair;
    // p_pair = malloc(sizeof(struct pair));


    // Set loadbalancer to listen for incoming requests
    if ((listenfd = server_listen(listenport)) < 0)
        err(1, "failed listening");

    int next = 0;
    while (true) {

        // printf("[+] load balancer waiting...\n");
        if ((clientfd = accept(listenfd, NULL, NULL)) < 0)
            err(1, "failed accepting");

        // Choose next server to send request to
        next = choose_server(servers, num_servers);
        if (next == -1) {
            printf("[+] all servers are dead\n");
            send_500(clientfd);
            continue;
        }
        printf("[+] next:%d\n", servers[next].port);

        // Attempt to connect to given server
        // If failed to connect, choose next server until it works
        while ((serverfd = client_connect(servers[next].port)) < 0){
            printf("[+] failed to connect to %d\n", servers[next].port);
            servers[next].alive = false;
            next = choose_server(servers, num_servers);
            // printf("[+] attempting %d\n", servers[next].port);
        }

        // Give struct pointer the clientfd and server fd
        p_pair->fd1 = clientfd;
        p_pair->fd2 = serverfd;

        // bridge_loop(acceptfd, connfd);
        pthread_create(&thread_id, NULL, handle_task, p_pair);
        pthread_join(thread_id, NULL);

        // Increment server requests count
        // TODO: CHECK IF ERROR OR ENTRY AND INCREMENT
        servers[next].entries += 1;
        
        num_requests++;

        // Perform routine healthcheck
        // if (num_requests % R == 0 && num_requests != 0) {
        //     // if (p_health_pkg->auto_health == false){
        //         for (int i = 0; i < num_servers; i++) {
        //             // printf("[+] checking health of %d\n", servers[i].port);
        //             health_check_probe(&servers[i]);
        //             // printf("server %d: entries:%d errors:%d\n", servers[i].port, servers[i].entries, servers[i].errors);
        //         }
        //     // }
        // }
    }

    return EXIT_SUCCESS;
}
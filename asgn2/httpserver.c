#include "httpserver.h"

// void * handle_request (void* p_disObj);

/*
    \brief 1. Want to read in the HTTP message/ data coming in from socket
    \param client_sockd - socket file descriptor
    \param message - object we want to 'fill in' as we read in the HTTP message
*/
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
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

    // printf("method: %s\n", message->method);
    // printf("file: %s\n", message->filename);
    // printf("httpversion: %s\n", message->httpversion);
    // printf("content_len: %ld\n", message->content_length);
}

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
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

    // Check if a valid healthcheck, and logging enabled
    if (strcmp(message->filename, "healthcheck") == 0) {
        if (strcmp(message->method, "GET") == 0){
            if (message->log_fd != -1) {
                message->status_code = 200;
                // Data to be returned is a string ("%d\n%d", errors, entries)
                message->content_length = nDigits(health->errors) + 1 + nDigits(health->entries);
            } else {
                message->status_code = 404;
                message->status_code = 0;
            }
        } else {
            message->status_code = 403;
            message->content_length = 0;
        }
        return;
    }

    // Extract file stats
    struct stat info;
    int stat_ret = stat(message->filename, &info);

    // GET will send file data in construct_http_response()
    if (strcmp(message->method, "HEAD") == 0 || strcmp(message->method, "GET") == 0) {
        // Check file exists and permissions
        // and set status_code, content_len accordingly
        if (stat_ret == -1){
            printf("file error: %s: does not exist\n", message->filename);
            message->status_code = 404;
            // message->content_length = 0;

        } else if ((info.st_mode & S_IRUSR) != S_IRUSR){
            printf("file error: %s: bad permissions\n", message->filename);
            message->status_code = 403;
            // message->content_length = 0;

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
            // message->content_length = 0;
            return;
        } else {
            message->status_code = 200;
        }

        char double_crlf[5] = "\r\n\r\n";
        char* body;
        ssize_t bytes = 0, total_bytes = 0;
        
        int fd = open(message->filename, O_RDWR | O_TRUNC | O_CREAT, (mode_t)0666);

        // Write bytes that are in same message as header
        body = strstr((char*) message->buffer, double_crlf);
        if (body) {
            total_bytes += write(fd, body+4, message->content_length);
        }

        // Write rest of bytes received
        printf("content_length:%ld\n", message->content_length);
        printf("TB:%ld\n", total_bytes);
        while (total_bytes < message->content_length) {
            printf("ehre, client: %ld, bytes:%ld\n", client_sockd, bytes);
            bytes = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
            printf("bytes:%ld\n", bytes);
            printf("ehre1\n");
            // Check if error receiving data
            if ((bytes < BUFFER_SIZE) 
             && (bytes + total_bytes < message->content_length)){
                printf("error: receiving bytes\n");
                break;
            }
            write(fd, message->buffer, bytes);
            total_bytes += bytes;
            printf("TB:%ld\n", total_bytes);
        }
        close(fd);
        // message->content_length = 0;

    } else {
        printf("Method not implemented");   
        message->status_code = 400;
        message->content_length = 0;
    }
}

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
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

    health->entries++;
    if (message->status_code > 201) {
        health->errors++;
    }

    // Send header
    char reply[BUFFER_SIZE] = "";
    if (strcmp(message->method, "PUT") == 0){
        snprintf(reply, BUFFER_SIZE, "%s %d %s\r\nContent-Length: %d\r\n\r\n",
            message->httpversion, message->status_code, status_message, 0);
    } else {
        snprintf(reply, BUFFER_SIZE, "%s %d %s\r\nContent-Length: %ld\r\n\r\n",
            message->httpversion, message->status_code, status_message, message->content_length);
    }
    send(client_sockd, reply, strlen(reply), 0);
    memset(reply, 0, strlen(reply));
    
    // Send file data if valid GET request
    if (message->status_code == 200 
      && strcmp(message->method, "GET") == 0 ) {
        // if a healthcheck, send health data
        if (strcmp(message->filename, "healthcheck") == 0){
            sprintf(reply, "%d\n%d", health->errors, health->entries);
            printf("err:%d\nentr:%d\n", health->errors, health->entries);
            send(client_sockd, reply, strlen(reply), 0);
        } else {
            int fd = open(message->filename, O_RDONLY);
            ssize_t size = BUFFER_SIZE;
            while (size != 0) {
                size = read(fd, message->buffer, BUFFER_SIZE);
                send(client_sockd, message->buffer, size, 0);
            }
            close(fd);
        }

    }
}

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
void* handle_task(void* thread){
    struct worker* w_thread = (struct worker*)thread;

    struct httpObject message;
    message.log_fd = w_thread->log_fd;
    // the worker in a way is a consumer

    int rc = 0;
    while (true) {
        // printf("Thread [%d] is ready for a task\n", w_thread->id);
        // while we don't have a valid client socket id we wait
        rc = pthread_mutex_lock(w_thread->lock);
        if (rc) {
            perror("pthread_mutex_lock\n");
            pthread_exit(NULL);
        }
        while (w_thread->client_sockd < 0) {
            // sleep
            pthread_cond_wait(&w_thread->condition_var, w_thread->lock);
        }

        printf("Thread [%d] Handling request. Client [%d]\n", w_thread->id, w_thread->client_sockd);

        read_http_request(w_thread->client_sockd, &message);

        process_request(w_thread->client_sockd, &message, w_thread->p_health);

        construct_http_response(w_thread->client_sockd, &message, w_thread->p_health);

        printf("Thread [%d] Response Sent\n", w_thread->id);

        // Say that we are done
        w_thread->client_sockd = -1;
        printf("Thread [%d] done handling request\n\n", w_thread->id);

        if (message.log_fd != 0){
            write_log2(&message);
        }

        rc = pthread_mutex_unlock(w_thread->lock);
        if (rc) {
            perror("pthread_mutex_unlock\n");
            pthread_exit(NULL);
        }

        // pthread_cond_signal(&w_thread->available);
        w_thread->avail = true;
    }
}

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------

int main(int argc, char** argv) {
    /*
        Option Parsing
    */
    if (argc < 2 || argc > 6) {
        usage();
    }
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
    int log_fd = -1;
    if (logfile) {
        log_fd = open(logfile, O_RDWR | O_TRUNC | O_CREAT, 0644);
    }

    // If no port specified
    if (argc == optind) {
        usage();
    }
    port = atoi(argv[optind]);

    /*
        Create sockaddr_in with server information
    */
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
    check(setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)));

    /*
        Bind server address to socket that is open
    */
    check(bind(server_sockd, (struct sockaddr *) &server_addr, addrlen));

    /*
        Listen for incoming connections
    */
    check(listen(server_sockd, 5)); // 5 should be enough, if not use SOMAXCONN

    /*
        Connecting with a client
    */
    struct sockaddr client_addr;
    socklen_t client_addrlen;

    // Create and initialize server health object
    struct healthObject* p_health = malloc(sizeof(struct healthObject));
    p_health->entries = 0;
    p_health->errors = 0;

    /*
        Create and initialize worker threads
    */
   struct worker workers[numthreads];
   pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
   pthread_mutex_t dlock = PTHREAD_MUTEX_INITIALIZER;
   int is_error = 0;

   // Initialize workers
   for (int i = 0; i < numthreads; i++) {
        workers[i].id = i;
        workers[i].client_sockd = -1;
        workers[i].condition_var = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
        // workers[i].available = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
        workers[i].lock = &lock;
        workers[i].p_health = p_health;
        workers[i].avail = true;
        if (logfile) {
            workers[i].log_fd = log_fd;
        } else {
            workers[i].log_fd = -1;
        }

        is_error = pthread_create(&workers[i].worker_id, NULL, handle_task, (void*)&workers[i]);

        if (is_error) {
            fprintf(stderr, "Error creating thread\n");
            return EXIT_FAILURE;
        }
    }

    int count = 0;
    int target_thread = 0;
    while (true) {
        printf("[+] server is waiting...\n");

        int rc = pthread_mutex_lock(&dlock);
        if (rc) {
            perror("pthread_mutex_lock\n");
            pthread_exit(NULL);
        }

        /*
         * 1. Accept Connection
         */
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        printf("MAIN: accepted client_sock: %d\n", client_sockd);

        // Find next available thread;
        // give target_thread client socket and signal to start working
        while (true) {
            if (workers[target_thread].avail){
                workers[target_thread].avail = false;
                workers[target_thread].client_sockd = client_sockd;
                printf("MAIN: target thread: %d\n", target_thread);
                pthread_cond_signal(&workers[target_thread].condition_var);
                break;
            } else {
                count++;
                target_thread = count % numthreads;
            }
        }

        rc = pthread_mutex_unlock(&dlock);
        if (rc) {
            perror("pthread_mutex_unlock\n");
            pthread_exit(NULL);
        }
    }
    return EXIT_SUCCESS;
}
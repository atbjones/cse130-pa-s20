#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

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
#include <errno.h>
#include <pthread.h>

#define BUFFER_SIZE 4096

extern int errno;

struct httpObject {
    char method[5];         // PUT, HEAD, GET
    char filename[28];      // what is the file we are worried about
    char httpversion[9];    // HTTP/1.1
    ssize_t content_length; // example: 13
    int status_code;
    uint8_t buffer[BUFFER_SIZE];
    // bool logging;
    int log_fd;
};

struct healthObject {
    int entries;
    int errors;
};

struct fooObject {
    // struct httpObject message;
    // struct healthObject health;
    int client_sockd;
    char* logfile;
};

void usage(){
    printf("usage: ./httpserver <port> -N <#threads> -l <log-file>\n");
    exit(EXIT_FAILURE);
}

bool filenamecheck(char* filename) {
    char c;
    for (size_t i = 0; i < strlen(filename); i++) {
        c = filename[i];
        if((c < '0' && c != '-') ||
        (c < 'A' && c > '9') ||
        (c < 'a' && c > 'Z' && c != '_') ||
        (c > 'z') ||
        i > 27){
            return false;
        }
    }
    return true;
}

// https://www.geeksforgeeks.org/program-count-digits-integer-3-different-methods/
// Returns the number of digits of a given number
int nDigits(long long n) 
{ 
    int count = 0; 
    while (n != 0) { 
        n = n / 10; 
        ++count; 
    } 
    return count; 
} 

void write_log (struct httpObject* message) {
    char buff[BUFFER_SIZE] = "";

    if (message->status_code <= 201) {
        // Write first line
        snprintf(buff, BUFFER_SIZE, "%s /%s length %ld\n", 
            message->method, message->filename, message->content_length);
        write(message->log_fd, buff, strlen(buff));
        memset(buff, 0, strlen(buff));

        // Write transferred file data
        if (strcmp(message->method, "HEAD") != 0){
            int fd = open(message->filename, O_RDONLY);
            ssize_t nbytes = BUFFER_SIZE, total_bytes = 0;
            while (nbytes != 0) {
                nbytes = read(fd, message->buffer, BUFFER_SIZE);
                // send(client_sockd, message->buffer, size, 0);
                for (int i = 0; i < nbytes; i += 20){ // Outer loop writes a line of x-fered data
                    snprintf(buff + strlen(buff), BUFFER_SIZE, "%08d ", (int)total_bytes + i);
                    for (int j = i; j < i + 20; j++) { // Inner loop writes data one byte at a time
                        if (j == nbytes-1) {
                            break;
                        }
                        snprintf(buff + strlen(buff), BUFFER_SIZE, "%02x ", message->buffer[j]);
                    }
                    snprintf(buff + strlen(buff), BUFFER_SIZE, "\n");
                    write(message->log_fd, buff, strlen(buff));
                    memset(buff, 0, strlen(buff));
                }
                total_bytes += nbytes;
            }
            close(fd);
        }
    } else {
        snprintf(buff, BUFFER_SIZE-1, "FAIL: %s /%s %s --- response %d\n", 
            message->method, message->filename, message->httpversion, message->status_code);
        write(message->log_fd, buff, strlen(buff));
    }

    write(message->log_fd, "========\n", 9);
}

#endif

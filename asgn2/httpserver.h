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
#include <limits.h>

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

struct worker {
    int id;
    int client_sockd;
    pthread_t worker_id;
    struct httpObject message;
    struct healthObject* p_health;
    pthread_cond_t condition_var;
    pthread_cond_t available;
    pthread_mutex_t* lock;
};

void usage(){
    printf("usage: ./httpserver <port> -N <#threads> -l <log-file>\n");
    exit(EXIT_FAILURE);
}

int check (int value) {
    if (value < 0){
        return EXIT_FAILURE;
    }
    return value;
}

bool filenamecheck (char* filename) {
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
    struct stat info;
    stat(message->filename, &info);
    if (message->status_code <= 201) {
        // Write first line
        snprintf(buff, BUFFER_SIZE, "%s /%s length %ld\n", 
            message->method, message->filename, info.st_size);
        write(message->log_fd, buff, strlen(buff));
        memset(buff, 0, strlen(buff));

        // Write transferred file data if (GET and not healthcheck) or PUT
        if (
            (strcmp(message->method, "GET") == 0 
            && strcmp(message->filename, "healthcheck") != 0)
            || strcmp(message->method, "PUT") == 0)
        {
            int fd = open(message->filename, O_RDONLY);
            ssize_t nbytes = BUFFER_SIZE, total_bytes = 0;
            while (nbytes != 0) {
                nbytes = read(fd, message->buffer, BUFFER_SIZE);
                // Outer loop writes a line of x-fered data
                for (int i = 0; i < nbytes; i += 20){
                    snprintf(buff + strlen(buff), BUFFER_SIZE, "%08d", (int)total_bytes + i);
                    // Inner loop writes data one byte at a time
                    for (int j = i; j < i + 20; j++) {
                        if (j == nbytes-1) {
                            break;
                        }
                        snprintf(buff + strlen(buff), BUFFER_SIZE, " %02x", message->buffer[j]);
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

void write_log2 (struct httpObject* message, off_t offset) {
    const int BUFFER_SIZE_MOD_20 = BUFFER_SIZE - BUFFER_SIZE % 20;
    printf("bufM2:%d\n", BUFFER_SIZE_MOD_20);
    printf("length %ld\n", message->content_length);
    char buff[BUFFER_SIZE - BUFFER_SIZE % 20] = "";
    if (message->status_code <= 201) {
        // Write first line
        sprintf(buff, "%s /%s length %ld\n", 
            message->method, message->filename, message->content_length);
        write(message->log_fd, buff, strlen(buff));
        memset(buff, 0, strlen(buff));

        // Write transferred file data if (GET and not healthcheck) or PUT
        if (
            (strcmp(message->method, "GET") == 0 
            && strcmp(message->filename, "healthcheck") != 0)
            || strcmp(message->method, "PUT") == 0)
        {
            int fd = open(message->filename, O_RDONLY);
            ssize_t nbytes = 1, total_bytes = 0;
            while (nbytes != 0) {
                nbytes = read(fd, message->buffer, BUFFER_SIZE_MOD_20);
                // Outer loop writes a line of x-fered data
                for (int i = 0; i < nbytes; i += 20){
                    sprintf(buff + strlen(buff), "%08d", (int)total_bytes + i);
                    // Inner loop writes data one byte at a time
                    for (int j = i; j < i + 20; j++) {
                        if (j == nbytes) {
                            break;
                        }
                        sprintf(buff + strlen(buff), " %02x", message->buffer[j]);
                    }
                    sprintf(buff + strlen(buff), "\n");
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

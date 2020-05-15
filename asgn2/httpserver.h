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
    int log_fd;
};

struct healthObject {
    int entries;
    int errors;
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

// Function to write first line of a log entry
int init_log_entry (struct httpObject* message) {
    char log_buff[BUFFER_SIZE] = ""; // Create log buffer
    // Write first line to log, then empty buffer
    if (message->status_code <= 201) {
        snprintf(log_buff, BUFFER_SIZE-1, "%s /%s length %ld\n", 
            message->method, message->filename, message->content_length);
        write(message->log_fd, log_buff, strlen(log_buff));
    } else {
        snprintf(log_buff, BUFFER_SIZE-1, "FAIL: %s /%s %s --- response %d\n", 
            message->method, message->filename, message->httpversion, message->status_code);
        write(message->log_fd, log_buff, strlen(log_buff));
    }
    memset(log_buff, 0, strlen(log_buff));
    return 0;
}

// Function to format and write data 
int write_log_data (char log_buff[], struct httpObject* message, ssize_t bytes, ssize_t total_bytes ) {
    for (int i = 0; i < bytes; i += 20){ // Outer loop writes a line of x-fered data
        snprintf(log_buff + strlen(log_buff), BUFFER_SIZE-1, "%08d ", (int)total_bytes + i);
        for (int j = i; j < i + 20; j++) { // Inner loop writes data one byte at a time
            if (j == bytes-1) {
                break;
            }
            snprintf(log_buff + strlen(log_buff), BUFFER_SIZE-1, "%02x ", message->buffer[j]);
        }
        snprintf(log_buff + strlen(log_buff), BUFFER_SIZE, "\n");
        write(message->log_fd, log_buff, strlen(log_buff));
        memset(log_buff, 0, strlen(log_buff));
    }
    return 0;
}

int end_log_entry (struct httpObject* message){
    write(message->log_fd, "========\n", 9);
    return 0;
}

#endif

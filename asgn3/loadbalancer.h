#ifndef __LOADBALANCER_H__
#define __LOADBALANCER_H__

#include<err.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include<pthread.h>

#define BUFFER_SIZE 4096

static const char health_check_request[] = "GET /healthcheck HTTP/1.1\r\n\r\n";

const char* whitespace = " \r\t\n\v\f";

struct serverObject {
    int id;
    int port;
    // int fd;
    int entries;
    int errors;
    bool alive;
    // uint8_t buff[BUFFER_SIZE];
};

struct health_thread_Object {
    struct serverObject* p_servers;
    int* p_num_reqs;
    int num_servers;
    // bool* auto_health;
};

struct pair {
    int fd1;
    int fd2;
};

void usage() {
    fprintf(stderr, "Usage: loadbalancer [-RN] [port_to_connect] [port_to_listen]\n");
    exit(EXIT_FAILURE);
}

int send_500 (int fd) {
    char buff[100] = "HTTP/1.1 500 Internal server error\r\nContent-Length: 0\r\n\r\n";
    int n = send(fd, buff, strlen(buff), 0);
    if (n < 0) {
        printf("send_500 error send())\n");
        return -1;
    }
    return 0;
}

#endif
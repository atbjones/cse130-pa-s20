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

#define BUFFER_SIZE 4096

static const char health_check_request[] = "GET /healthcheck HTTP/1.1\r\n\r\n";

const char* whitespace = " \r\t\n\v\f";

struct serverObject {
    int id;
    int port;
    // int fd;
    int entries;
    int errors;
    uint8_t buff[BUFFER_SIZE];
};

void usage() {
    fprintf(stderr, "Usage: loadbalancer [-RN] [port_to_connect] [port_to_listen]\n");
    exit(EXIT_FAILURE);
}

#endif
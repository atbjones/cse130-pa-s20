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

bool file_exists(char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd != -1){
        return true;
    }
    return false;
}

#endif

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

#define BUFFER_SIZE 4096

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
    close(fd);
    return false;
}

int find_double_crlf(uint8_t* buff){
    // printf("----------In double_crlf\n");

    char str_buff[BUFFER_SIZE];
    memcpy(str_buff, buff, BUFFER_SIZE);
    int len = strlen(str_buff);

    // printf("Printing Buffer...\n");
    // for (ssize_t i = 0; i < len; i++) {
    //     printf("%c", str_buff[i]);
    // }
    // printf("str_buff: %s")

    int i = 0;
    while (i < len - 3){
      if (str_buff[i] == '\r'){
         if (str_buff[i+1] == '\n'){
            if (str_buff[i+2] == '\r'){
               if (str_buff[i+3] == '\n'){
                   return i + 4;
                //   printf("here %c\n", buff[i+4]);
               }
            }
         }

      }
      i++;
   }
   printf("ERROR: Double clrf not found\n");
   return -1;
}

#endif

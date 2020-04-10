#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 1024
extern int errno;

void readstdin() {
   char c;
   while (c != EOF) {
      c = getchar();
      if (c != EOF) putchar(c);
   }
}

void readfile(int fd){
   char buf[BUFFER_SIZE];
   ssize_t size = BUFFER_SIZE;
   while (size != 0) {
      size = read(fd, buf, BUFFER_SIZE);
      //printf("size: %ld\n", size);
      for(int i = 0; i < size; i++){
         printf("%c", buf[i]);
      }
   }
}

int main(int argc, char* argv[]) {
   if (argc == 1){
      readstdin();
   } else {
      //read each arg
      for (int i = argc-1; i > 0; i--){
         if (strcmp(argv[i], "-") == 0) {
            readstdin();
            continue;
         }
         //printf("Open file: %s\n", argv[i]);
         int fd = open(argv[i], O_RDONLY);
         if (fd != -1) {
            readfile(fd);
         } else {
            fprintf(stderr, "dog: %s: %s\n", argv[i], strerror(errno));
         }
         close(fd);
      }
   }
   return 0;
}

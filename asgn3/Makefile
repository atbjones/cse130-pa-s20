CC = gcc
CFLAGS = -pthread -g -Wall -Wextra -Wpedantic -Wshadow
TARGET = loadbalancer

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET) log

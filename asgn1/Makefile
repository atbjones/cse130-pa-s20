CC = gcc
CFLAGS = -g -Wall -Wextra -Wpedantic -Wshadow
TARGET = httpserver

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET)

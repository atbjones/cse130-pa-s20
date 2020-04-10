CC = gcc
CFLAGS = -g -Wall -Wextra -Wpedantic -Wshadow
TARGET = dog

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET)

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = mysh
SRC = mysh.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -pthread -I../shared
SRC = client.c menu.c ffmpeg_commands.c
OBJ = $(SRC:.c=.o)
TARGET = client

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
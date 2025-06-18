CC=gcc
CFLAGS=-Wall -Wextra -I../common
LDFLAGS=-luuid
SRC=client.c
OBJ=$(SRC:.c=.o)
TARGET=client

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
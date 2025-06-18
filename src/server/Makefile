CC=gcc
CFLAGS=-Wall -Wextra -pthread
LDFLAGS=-luuid
SRC=server.c client_thread.c upload_thread.c hashmap.c minheap.c
OBJ=$(SRC:.c=.o)
TARGET=server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
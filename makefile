CC = gcc
CFLAGS =
COMMON_FILES = common.c
CLIENT_DIR = .
SERVER_DIR = .

all: user server

user: user.c $(COMMON_FILES)
	$(CC) $(CFLAGS) -o $@ user.c $(COMMON_FILES)

server: server.c $(COMMON_FILES)
	$(CC) $(CFLAGS) -o $@ server.c $(COMMON_FILES)

clean:
	rm -f user server

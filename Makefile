CC = gcc
CFLAGS = -Wall -Wextra -pthread -lrt

SRC = src/main.c \
      src/master.c \
      src/worker.c \
      src/http.c \
      src/thread_pool.c \
      src/cache.c \
      src/logger.c \
      src/stats.c \
      src/config.c

BIN = server

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	rm -f $(BIN)

run: $(BIN)
	./$(BIN)
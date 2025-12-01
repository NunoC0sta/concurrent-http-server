CC = gcc
CFLAGS = -Wall -Wextra -pthread -lrt

SRC = src/main.c src/master.c src/config.c
BIN = server

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	rm -f $(BIN)

run: $(BIN)
	./$(BIN)

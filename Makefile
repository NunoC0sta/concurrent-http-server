CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -lrt

SRC = $(wildcard src/*.c) 

BIN = server

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)

run: $(BIN)
	./$(BIN)

.PHONY: all clean run
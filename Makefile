CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I src
LDFLAGS = -lrt -pthread

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = server

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN) src/*.o

run: $(BIN)
	./$(BIN)
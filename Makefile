CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I src
LDFLAGS = -lrt -pthread
TEST_LDFLAGS = -lcurl -pthread

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = server
TEST_BIN = tests/test_concurrent

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_BIN): tests/test_concurrent.c
	$(CC) $(CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

clean:
	rm -f $(BIN) src/*.o $(TEST_BIN)

run: $(BIN)
	./$(BIN)
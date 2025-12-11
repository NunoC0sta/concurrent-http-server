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

test_functional: tests/test_functional.c
	gcc -Wall -Wextra -o tests/test_functional tests/test_functional.c -lcurl

test_concurrent: tests/test_concurrent.c
	gcc -Wall -Wextra -pthread -o tests/test_concurrent tests/test_concurrent.c -lcurl -lpthread

test_synchronization: tests/test_synchronization.c
	gcc -Wall -Wextra -pthread -o tests/test_synchronization tests/test_synchronization.c -lcurl -lpthread

test_stress: tests/test_stress.c
	gcc -Wall -Wextra -o tests/test_stress tests/test_stress.c -lcurl

tests: test_functional test_concurrent test_synchronization test_stress
	@echo "All test executables built successfully"

run_tests: tests
	@echo "Running Functional Tests..."
	./tests/test_functional
	@echo ""
	@echo "Running Concurrency Tests..."
	./tests/test_concurrent
	@echo ""
	@echo "Running Synchronization Tests..."
	./tests/test_synchronization
	@echo ""
	@echo "Running Stress Tests (manual verification required)..."
	./tests/test_stress

clean_tests:
	rm -f tests/test_functional tests/test_concurrent tests/test_synchronization tests/test_stress

clean_all: clean clean_tests

.PHONY: test_functional test_concurrent test_synchronization test_stress tests run_tests clean_tests
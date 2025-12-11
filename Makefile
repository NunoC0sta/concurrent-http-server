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

valgrind: $(TARGET)
	@echo "========================================================"
	@echo "  A CORRER SERVIDOR COM VALGRIND (Verificação de Fugas de Memória)"
	@echo "========================================================"
	@echo "PASSOS SEGUINTES:"
	@echo "  1. Abrir um NOVO TERMINAL."
	@echo "  2. Executar um teste de carga: ab -n 5000 -c 100 http://localhost:8080/"
	@echo "  3. Verificar a saída do Valgrind: deve reportar 'definitely lost: 0 bytes'."
	valgrind --leak-check=full --show-leak-kinds=all ./server

helgrind: $(TARGET)
	@echo "========================================================"
	@echo "  A CORRER SERVIDOR COM HELGRIND (Verificação de Race Conditions)"
	@echo "========================================================"
	@echo "PASSOS SEGUINTES:"
	@echo "  1. Abrir um NOVO TERMINAL."
	@echo "  2. Executar um teste de carga: ab -n 5000 -c 100 http://localhost:8080/"
	@echo "  3. Verificar a saída do Helgrind: não deve reportar avisos de 'potential data race'."
	valgrind --tool=helgrind ./server

clean_tests:
	rm -f tests/test_functional tests/test_concurrent tests/test_synchronization tests/test_stress

clean_all: clean clean_tests

.PHONY: test_functional test_concurrent test_synchronization test_stress tests run_tests clean_tests
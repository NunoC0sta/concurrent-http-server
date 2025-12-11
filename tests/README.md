# Test Suite Documentation

***

### 1. Test Suite Structure

The concurrent HTTP server is validated through a set of tests covering four main domains: **Functionality**, **Concurrency**, **Synchronization**, and **Stability**.

| Category | C File/Script | Main Objective |
| :--- | :--- | :--- |
| **Functional** | `test_functional.c` | Validation of the HTTP/1.1 protocol (Status Codes, MIME Types, File Serving). |
| **Concurrency** | `test_concurrent.c` / `test_load.sh` | Measurement of Performance and Robustness under Load. |
| **Synchronization** | `test_synchronization.c` | Thread Safety, Log Integrity, and Counter Consistency. |
| **Stress/IPC** | `test_stress.c` | Memory Leaks, Graceful Shutdown (`SIGTERM`), and IPC Resource Cleanup. |

### 2. Execution Commands

All tests are compiled via the main `Makefile` and must be run with the server (`./server`) active (except for Test 14, which shuts it down) alternatively you may run all tests (asides from the consistent load and load tests) via `Make run_tests` after the server is already running.

```bash
# Executes HTTP protocol tests (Tests 1-4)
make test_functional

# Executes multi-threaded load tests (Tests 5-8)
make test_concurrent

# Executes thread safety and data integrity tests (Tests 9-12)
make test_synchronization

# Executes stress and stability tests (Tests 13-15)
# WARNING: Test 14 shuts down the server process.
make test_stress

#Alternatively you may also run all tests at one by doing
make run_tests

#To Execute the test_consistent_load test
./tests/test_consistent_load.sh

#To Execute the test_load tests
./tests/test_load.sh
```
### 3. Quality Requirements Details

#### A. Functional Tests

* **Status Codes:** Tested for **200 OK** (existing files) and **404 Not Found** (invalid paths).
* **MIME Types:** Verification that the server sends the correct `Content-Type` headers (`text/html`, `text/plain`, etc.).

#### B. Concurrency and Robustness Tests

* **Robustness:** Load tests (`test_concurrent.c`) must report **zero dropped connections** (`failed_requests` = 0) under high concurrency.
* **Performance:** Measurement of Requests/sec and Average Response Latency metrics.

#### C. Synchronization Tests

* **Log Integrity:** **Test 10** verifies that concurrent entries in the `access.log` are not corrupted or interleaved, proving correct synchronization mechanism usage in file I/O.
* **Counters:** **Test 12** validates that the statistics counters (exposed at `/stats`) are updated atomically and accurately.

### 4. Quality Analysis (Valgrind and Helgrind)

The absence of memory errors and concurrency issues is a critical project requirement, verified with Valgrind tools.

#### 1. Memory Leaks

* **Verification Command:**
    ```bash
    make valgrind

    #Alternatively you may run this command
    valgrind --leak-check=full --show-leak-kinds=all ./server
    ```
* **Procedure:** Generate intense load (`ab -n 5000 -c 100 http://localhost:8080/`) while Valgrind is running.
* **Success Criterion:** The output must indicate **`0 bytes in 0 blocks are definitely lost`**.

#### 2. Race Conditions

* **Verification Command:**
    ```bash
    make helgrind

    #Alternatively you may run this command
    valgrind --tool=helgrind ./server
    ```
* **Procedure:** Generate intense load on the server.
* **Success Criterion:** Helgrind **must not report** "potential data race" warnings.

### 5. Stability Requirements

* **Graceful Shutdown (Test 14):** Receiving the `SIGTERM` signal must lead to a clean shutdown of all *worker threads* and processes in a timely manner (generally under 3 seconds).
* **Zombie Processes:** The system must be checked after shutdown to ensure no `<defunct>` (zombie) server processes remain.
* **IPC Cleanup:** Shared IPC resources (shared memory, semaphores) must be properly cleaned up after shutdown.
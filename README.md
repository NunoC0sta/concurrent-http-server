# Multi-Threaded Web Server with IPC and Semaphores
**Sistemas Operativos - TP2**

A production-level concurrent HTTP/1.1 web server, implementing advanced process and thread synchronization using POSIX semaphores, shared memory, and thread pools.

## Table of Contents
Overview
Quick Start
Features
Project Structure
Configuration
Testing
Implementation Details
Authors

## Overview
This project implements a multi-process and multi-threaded HTTP/1.1 web server that demonstrates:
* **Process Management:** Master-Worker architecture using fork().
* **Inter-Process Communication (IPC):** Shared memory and POSIX semaphores.
* **Thread Synchronization:** Pthread mutexes, condition variables, and reader-writer locks.
* **Concurrent Request Handling:** Thread pools using the producer-consumer pattern.
* **HTTP Protocol:** HTTP/1.1 support including GET and HEAD methods.
* **Resource Management:** Thread-safe LRU file cache and statistics tracking.

## Quick Start

### 1. Compilation
Compile the server using Make:
make clean && make

### 2. Run Server
Start the server on the default port (8080):
./server

### 3. Access
Browser: http://localhost:8080
Curl: curl -v http://localhost:8080/index.html


### 4. Features
Core Features:

    [x] Multi-Process Architecture: 1 Master + N Workers.

    [x] Thread Pools: Producer-Consumer pattern.

    [x] HTTP/1.1 Support: GET and HEAD methods.

    [x] Status Codes: 200, 404, 403, 500, 503.

    [x] MIME Types: HTML, CSS, JavaScript, Images (PNG, JPG), PDF.

    [x] Custom Error Pages: Custom 404 and 500 pages.

Synchronization Features

    [x] POSIX Semaphores: Synchronization between processes.

    [x] Pthread Mutexes: Thread-level mutual exclusion.

    [x] Condition Variables: Signaling for the connection queue.

    [x] Read-Write Locks (RW Locks): Safe access to the file cache.

Bonus Features

    [x] Real-Time Dashboard (5 points)

    [x] Virtual Hosts (4 points)

    [x] Range Requests / Partial Content (3 points)

    [] HTTP Keep-Alive

    [ ] CGI Suporte

    [ ] HTTPS Suporte


### 5. Project Structure
src/
    main.c
    master.c/h  
    worker.c/h
    http.c/h
    thread_pool.c/h
    cache.c/h
    logger.c/h
    stats.c/h
    config.c/h
docs/
    design.pdf
    user_manual.pdf
    technicaldoc.pdf
tests/
    test_concurrent.c
    test_load.sh
    README.md
www/
    index.html
    errors/
        404.html
        500.html
    Images/
Makefile
server.conf
README.md

### 6. Testing

#### 6.1. Test Suite Structure

The concurrent HTTP server is validated through a set of tests covering four main domains: **Functionality**, **Concurrency**, **Synchronization**, and **Stability**.

| Category | C File/Script | Main Objective |
| :--- | :--- | :--- |
| **Functional** | `test_functional.c` | Validation of the HTTP/1.1 protocol (Status Codes, MIME Types, File Serving). |
| **Concurrency** | `test_concurrent.c` / `test_load.sh` | Measurement of Performance and Robustness under Load. |
| **Synchronization** | `test_synchronization.c` | Thread Safety, Log Integrity, and Counter Consistency. |
| **Stress/IPC** | `test_stress.c` | Memory Leaks, Graceful Shutdown (`SIGTERM`), and IPC Resource Cleanup. |

#### 6.2. Execution Commands

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
#### 6.3. Quality Requirements Details

##### A. Functional Tests

* **Status Codes:** Tested for **200 OK** (existing files) and **404 Not Found** (invalid paths).
* **MIME Types:** Verification that the server sends the correct `Content-Type` headers (`text/html`, `text/plain`, etc.).

##### B. Concurrency and Robustness Tests

* **Robustness:** Load tests (`test_concurrent.c`) must report **zero dropped connections** (`failed_requests` = 0) under high concurrency.
* **Performance:** Measurement of Requests/sec and Average Response Latency metrics.

##### C. Synchronization Tests

* **Log Integrity:** **Test 10** verifies that concurrent entries in the `access.log` are not corrupted or interleaved, proving correct synchronization mechanism usage in file I/O.
* **Counters:** **Test 12** validates that the statistics counters (exposed at `/stats`) are updated atomically and accurately.

#### 6.4. Quality Analysis (Valgrind and Helgrind)

The absence of memory errors and concurrency issues is a critical project requirement, verified with Valgrind tools.

##### 1. Memory Leaks

* **Verification Command:**
    ```bash
    make valgrind

    #Alternatively you may run this command
    valgrind --leak-check=full --show-leak-kinds=all ./server
    ```
* **Procedure:** Generate intense load (`ab -n 5000 -c 100 http://localhost:8080/`) while Valgrind is running.
* **Success Criterion:** The output must indicate **`0 bytes in 0 blocks are definitely lost`**.

##### 2. Race Conditions

* **Verification Command:**
    ```bash
    make helgrind

    #Alternatively you may run this command
    valgrind --tool=helgrind ./server
    ```
* **Procedure:** Generate intense load on the server.
* **Success Criterion:** Helgrind **must not report** "potential data race" warnings.

#### 6.5. Stability Requirements

* **Graceful Shutdown (Test 14):** Receiving the `SIGTERM` signal must lead to a clean shutdown of all *worker threads* and processes in a timely manner (generally under 3 seconds).
* **Zombie Processes:** The system must be checked after shutdown to ensure no `<defunct>` (zombie) server processes remain.
* **IPC Cleanup:** Shared IPC resources (shared memory, semaphores) must be properly cleaned up after shutdown.

### 7. Implementation Details
Master Process

    Initializes the listening socket.

    Creates shared memory and semaphores.

    Forks N worker processes.

    Accepts connections and places them into the shared queue (Producer).

Worker Processes

    Maintains a fixed-size thread pool.

    Threads retrieve connections from the shared queue (Consumer).

    Atomically updates shared statistics.

    Caches files in memory using an LRU algorithm protected by Read-Write locks.


### 8. Known Issues
 
Currently there are no known Issues

### Authors

    Martim Travesso Dias

        Nº Mec: 125925

        Email: martimtdias@ua.pt

    Nuno Carvalho Costa

        Nº Mec: 125120

        Email: nunoc27@ua.pt


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

Fazer quando tivermos os testes

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
 
Fazer depois também

### Authors

    Martim Travesso Dias

        Nº Mec: 125925

        Email: martimtdias@ua.pt

    Nuno Carvalho Costa

        Nº Mec: 125120

        Email: nunoc27@ua.pt


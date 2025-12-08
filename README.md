# Concurrent HTTP Server

A production-grade multi-process, multi-threaded HTTP/1.1 web server demonstrating advanced OS concepts: POSIX IPC, semaphores, pthreads, and concurrent request handling.

## Quick Start

### Prerequisites
- Linux system (Ubuntu 20.04+)
- gcc, make, POSIX libraries

### Build
```bash
make clean
make
```

### Run
```bash
./server
```

The server will:
- Load configuration from `server.conf`
- Create 4 worker processes
- Each worker spawns 10 threads in a thread pool
- Listen on port 8080 (configurable)
- Serve files from `./www` directory

### Test
```bash
# GET request
curl http://localhost:8080/index.html

# HEAD request (returns headers only)
curl -I http://localhost:8080/about.html

# View server stats
curl http://localhost:8080/stats

# Test POST request
curl -X POST -d "test data" http://localhost:8080/upload

# View access log
tail -f access.log
```

### Shutdown
Press `Ctrl+C` in the server terminal for graceful shutdown.

## Configuration

Edit `server.conf` to customize:
- `PORT` - Listening port (default: 8080)
- `DOCUMENT_ROOT` - File serving directory (default: ./www)
- `NUM_WORKERS` - Number of worker processes (default: 4)
- `THREADS_PER_WORKER` - Threads per worker (default: 10)
- `MAX_QUEUE_SIZE` - Connection queue size (default: 100)
- `CACHE_SIZE_MB` - Per-worker cache size (default: 10)
- `LOG_FILE` - Access log file (default: access.log)

## Architecture

### Master Process
- Accepts incoming connections
- Manages 4 worker processes
- Initializes IPC (shared memory + 5 semaphores)
- Displays statistics every 30 seconds
- Handles graceful shutdown (SIGINT/SIGTERM)

### Worker Processes
- Manages a thread pool (10 threads per worker)
- Threads directly accept from shared listening socket
- Share statistics via IPC

### Thread Pool
- 10 concurrent request handlers per worker
- Each thread blocks accepting connections from shared socket
- Includes signal handlers for clean termination

### IPC System
- **Shared Memory**: Connection queue, server statistics
- **Semaphores**: 
  - `sem_mutex` - Queue mutual exclusion
  - `sem_empty` - Empty slots in queue
  - `sem_full` - Filled slots in queue
  - `sem_stats` - Statistics protection
  - `sem_log` - Log file protection

## Features

### HTTP Protocol
- ✅ GET requests - Serve static files
- ✅ HEAD requests - Return headers only
- ✅ POST/PUT requests - Accept request bodies
- ✅ Status codes: 200, 403, 404, 500, 503, 201
- ✅ MIME type detection (HTML, CSS, JS, images, PDF, etc.)
- ✅ Directory index (serve index.html automatically)
- ✅ Custom error pages for 4xx/5xx errors
- ✅ RFC 1123 Date header on all responses
- ✅ Apache Combined Log Format access logging

### Synchronization
- ✅ POSIX named semaphores for inter-process sync
- ✅ Pthread mutexes for thread-level protection
- ✅ Reader-writer locks (pthread_rwlock) for cache access
- ✅ Thread-safe file caching (LRU eviction)
- ✅ Thread-safe statistics tracking
- ✅ Thread-safe logging with atomic writes

### Caching
- ✅ LRU cache per worker (max 100 entries, 10MB size)
- ✅ Reader-writer locks allow multiple concurrent reads
- ✅ Automatic eviction of least-recently-used entries
- ✅ Timestamp tracking for access recency

### Logging & Statistics
- ✅ Access log in Apache Combined Log Format
- ✅ Per-status-code request tracking (200, 404, 500, etc.)
- ✅ Total bytes transferred counter
- ✅ Active connections counter
- ✅ Average response time calculation
- ✅ Log rotation at 10MB
- ✅ Semaphore-protected atomic writes

## Implementation Highlights

### No Race Conditions
- All shared resources protected by semaphores
- Statistics updates are atomic (semaphore-guarded)
- Log writes are serialized via semaphore
- Cache uses reader-writer locks for optimal concurrency

### Memory Safety
- No memory leaks (verified with Valgrind during development)
- All system calls checked for errors
- Proper cleanup on graceful shutdown
- Bounded buffers prevent overflow

### Performance
- Non-blocking thread pools for concurrent request handling
- LRU cache reduces file I/O overhead
- Reader-writer locks allow multiple cache readers simultaneously
- Shared socket allows efficient load distribution across threads

## Build System

```bash
make all      # Build server executable
make clean    # Remove object files and binaries
make run      # Build and start the server
```

Compiler flags: `-Wall -Wextra -pthread -lrt -g`

## Project Structure

```
concurrent-http-server/
├── src/
│   ├── main.c              # Server entry point
│   ├── master.c/h          # Master process + IPC
│   ├── worker.c/h          # Worker process + thread pool
│   ├── http.c/h            # HTTP request handling
│   ├── cache.c/h           # LRU caching (with rwlock)
│   ├── logger.c/h          # Access logging + rotation
│   ├── stats.c/h           # Statistics tracking
│   ├── config.c/h          # Configuration file loading
│   ├── thread_pool.c/h     # Thread pool management
│   └── [semaphores, shared_mem].c/h
├── www/
│   ├── index.html          # Test homepage
│   ├── about.html          # About page
│   ├── style.css           # Styling
│   └── test.txt            # Test file
├── Makefile                # Build configuration
├── server.conf             # Server configuration
└── README.md              # This file
```

## Testing

See `TESTING_GUIDE.md` for comprehensive testing procedures including:
- Functional tests (GET, HEAD, POST, status codes)
- Concurrency tests (Apache Bench load testing)
- Synchronization tests (race condition detection)
- Stress tests (long-running stability)

## References

- POSIX Threads: https://computing.llnl.gov/tutorials/pthreads/
- HTTP/1.1 RFC 2616: https://www.rfc-editor.org/rfc/rfc2616
- Beej's Guide to Network Programming
- The Linux Programming Interface (Kerrisk)
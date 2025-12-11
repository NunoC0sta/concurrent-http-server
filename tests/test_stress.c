#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>

#define SERVER_URL "http://localhost:8080"

int tests_run = 0, tests_passed = 0, tests_failed = 0;

typedef struct {
    int id;
    int num_requests;
    int *success_count;
    int *failure_count;
    pthread_mutex_t *lock;
    volatile int *stop_flag;
} load_thread_args_t;

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    (void)contents; (void)userp;
    return size * nmemb;
}

int check_server(void) {
    CURL* curl = curl_easy_init();
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

pid_t get_server_pid(void) {
    FILE* fp = popen("pgrep -x server", "r");
    if (!fp) return -1;
    
    pid_t pid = -1;
    if (fscanf(fp, "%d", &pid) != 1) {
        pid = -1;
    }
    pclose(fp);
    return pid;
}

long get_process_memory(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    FILE* fp = fopen(path, "r");
    if (!fp) return -1;
    
    char line[256];
    long vmrss = -1;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &vmrss);
            break;
        }
    }
    fclose(fp);
    return vmrss; // in KB
}

int check_zombie_processes(void) {
    FILE* fp = popen("ps aux | grep -c '[d]efunct.*server'", "r");
    if (!fp) return -1;
    
    int count = 0;
    if (fscanf(fp, "%d", &count) != 1) {
        count = -1;
    }
    pclose(fp);
    return count;
}

int check_shm_cleanup(void) {
    int count = 0;
    DIR* dir = opendir("/dev/shm");
    if (!dir) return -1;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "concurrent_http")) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

void* load_thread_fn(void* arg) {
    load_thread_args_t* args = (load_thread_args_t*)arg;
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    int local_success = 0, local_failure = 0;
    
    for (int i = 0; i < args->num_requests && !(*args->stop_flag); i++) {
        char url[256];
        snprintf(url, sizeof(url), "%s/index.html?req=%d", SERVER_URL, i);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200) {
                local_success++;
            } else {
                local_failure++;
            }
        } else {
            local_failure++;
        }
        
        usleep(10000); // 10ms between requests
    }
    
    pthread_mutex_lock(args->lock);
    *args->success_count += local_success;
    *args->failure_count += local_failure;
    pthread_mutex_unlock(args->lock);
    
    curl_easy_cleanup(curl);
    return NULL;
}

void test_sustained_load(void) {
    printf("\n[TEST 13] Sustained load test\n");
    
    pid_t server_pid = get_server_pid();
    if (server_pid < 0) {
        printf("  ✗ FAILED - Cannot find server process\n");
        tests_failed++;
        tests_run++;
        return;
    }
    
    printf("  Server PID: %d\n", server_pid);
    printf("  Starting 30-second load test with 20 concurrent clients...\n");
    
    // Measure initial memory
    long initial_mem = get_process_memory(server_pid);
    if (initial_mem < 0) {
        printf("  ✗ FAILED - Cannot read memory usage\n");
        tests_failed++;
        tests_run++;
        return;
    }
    printf("  Initial memory: %ld KB\n", initial_mem);
    
    // Create load threads
    int num_threads = 20;
    int requests_per_thread = 150; // 20 threads × 150 = 3000 requests over 30s
    pthread_t threads[num_threads];
    load_thread_args_t args[num_threads];
    
    int success_count = 0, failure_count = 0;
    volatile int stop_flag = 0;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Start threads
    for (int i = 0; i < num_threads; i++) {
        args[i].id = i;
        args[i].num_requests = requests_per_thread;
        args[i].success_count = &success_count;
        args[i].failure_count = &failure_count;
        args[i].lock = &lock;
        args[i].stop_flag = &stop_flag;
        pthread_create(&threads[i], NULL, load_thread_fn, &args[i]);
    }
    
    // Monitor for 30 seconds
    int server_crashed = 0;
    for (int i = 0; i < 30; i++) {
        sleep(1);
        if (kill(server_pid, 0) != 0) {
            printf("  ✗ FAILED - Server crashed during load test at %d seconds!\n", i + 1);
            server_crashed = 1;
            stop_flag = 1;
            break;
        }
    }
    
    // Stop and join threads
    stop_flag = 1;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    gettimeofday(&end, NULL);
    long duration_ms = (end.tv_sec - start.tv_sec) * 1000 + 
                       (end.tv_usec - start.tv_usec) / 1000;
    
    // Check final memory
    long final_mem = get_process_memory(server_pid);
    long mem_increase = (final_mem > 0 && initial_mem > 0) ? 
                        (final_mem - initial_mem) : -1;
    
    printf("  Duration: %ld ms\n", duration_ms);
    printf("  Successful requests: %d\n", success_count);
    printf("  Failed requests: %d\n", failure_count);
    printf("  Total requests: %d\n", success_count + failure_count);
    
    if (final_mem > 0) {
        printf("  Final memory: %ld KB\n", final_mem);
        printf("  Memory increase: %ld KB\n", mem_increase);
    }
    
    // STRICT PASS CRITERIA: 100% success, server alive, reasonable memory
    int total_requests = success_count + failure_count;
    int all_succeeded = (failure_count == 0 && total_requests > 0);
    int server_alive = (kill(server_pid, 0) == 0);
    int memory_ok = (mem_increase >= 0 && mem_increase < 100000); // Less than 100MB increase
    
    if (server_crashed) {
        printf("  ✗ FAILED - Server crashed\n");
        tests_failed++;
    } else if (!all_succeeded) {
        printf("  ✗ FAILED - Not all requests succeeded (%d failures)\n", failure_count);
        tests_failed++;
    } else if (!server_alive) {
        printf("  ✗ FAILED - Server not responding after test\n");
        tests_failed++;
    } else if (!memory_ok) {
        printf("  ✗ FAILED - Excessive memory growth (%ld KB)\n", mem_increase);
        tests_failed++;
    } else {
        printf("  ✓ PASSED - 100%% success rate, server stable, memory OK\n");
        tests_passed++;
    }
    tests_run++;
    
    pthread_mutex_destroy(&lock);
}

void test_memory_leaks(void) {
    printf("\n[TEST 14] Memory leak detection\n");
    
    pid_t server_pid = get_server_pid();
    if (server_pid < 0) {
        printf("  ✗ FAILED - Cannot find server process\n");
        tests_failed++;
        tests_run++;
        return;
    }
    
    printf("  Measuring memory over 5 request cycles...\n");
    
    long mem_samples[5];
    int valid_samples = 0;
    
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("  Cycle %d: Sending 200 requests...\n", cycle + 1);
        
        int failures = 0;
        for (int i = 0; i < 200; i++) {
            CURL* curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
                CURLcode res = curl_easy_perform(curl);
                if (res != CURLE_OK) failures++;
                curl_easy_cleanup(curl);
            }
        }
        
        if (failures > 0) {
            printf("  ✗ FAILED - %d requests failed in cycle %d\n", failures, cycle + 1);
            tests_failed++;
            tests_run++;
            return;
        }
        
        sleep(2); // Let things settle
        
        long mem = get_process_memory(server_pid);
        if (mem <= 0) {
            printf("  ✗ FAILED - Cannot read memory at cycle %d\n", cycle + 1);
            tests_failed++;
            tests_run++;
            return;
        }
        
        mem_samples[valid_samples++] = mem;
        printf("    Memory after cycle %d: %ld KB\n", cycle + 1, mem);
    }
    
    // Check if memory is growing unbounded
    long max_growth = 0;
    for (int i = 1; i < valid_samples; i++) {
        long growth = mem_samples[i] - mem_samples[0];
        if (growth > max_growth) max_growth = growth;
    }
    
    printf("  Maximum memory growth: %ld KB\n", max_growth);
    
    // STRICT: Memory must not grow more than 10MB across cycles
    if (max_growth < 10000) {
        printf("  ✓ PASSED - No memory leak detected (<%ld KB growth)\n", max_growth);
        tests_passed++;
    } else {
        printf("  ✗ FAILED - Memory leak detected (%ld KB growth exceeds 10MB limit)\n", max_growth);
        tests_failed++;
    }
    tests_run++;
}

void test_graceful_shutdown(void) {
    printf("\n[TEST 15] Graceful shutdown under load\n");
    
    printf("  ⚠ This test will terminate the server\n");
    
    pid_t server_pid = get_server_pid();
    if (server_pid < 0) {
        printf("  ✗ FAILED - Cannot find server process\n");
        tests_failed++;
        tests_run++;
        return;
    }
    
    printf("  Server PID: %d\n", server_pid);
    printf("  Starting background load...\n");
    
    // Start background load
    volatile int stop_load = 0;
    int success = 0, failure = 0;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    
    load_thread_args_t args = {
        .id = 0,
        .num_requests = 1000,
        .success_count = &success,
        .failure_count = &failure,
        .lock = &lock,
        .stop_flag = &stop_load
    };
    
    pthread_t load_thread;
    pthread_create(&load_thread, NULL, load_thread_fn, &args);
    
    // Wait for load to start
    sleep(2);
    
    printf("  Sending SIGTERM to server...\n");
    kill(server_pid, SIGTERM);
    
    // Wait for server to shutdown (max 5 seconds)
    int shutdown_clean = 0;
    int shutdown_time_ms = 0;
    for (int i = 0; i < 50; i++) {
        if (kill(server_pid, 0) != 0) {
            shutdown_time_ms = i * 100;
            shutdown_clean = 1;
            break;
        }
        usleep(100000); // 100ms
    }
    
    stop_load = 1;
    pthread_join(load_thread, NULL);
    
    if (!shutdown_clean) {
        printf("  ✗ FAILED - Server did not terminate within 5 seconds\n");
        kill(server_pid, SIGKILL); // Force kill
        tests_failed++;
        tests_run++;
        pthread_mutex_destroy(&lock);
        return;
    }
    
    printf("  Server terminated after %d ms\n", shutdown_time_ms);
    
    sleep(1); // Let cleanup happen
    
    // Check for zombie processes
    int zombies = check_zombie_processes();
    printf("  Zombie processes: %d\n", zombies);
    
    // Check for leftover shared memory
    int shm_count = check_shm_cleanup();
    printf("  Leftover /dev/shm files: %d\n", shm_count);
    
    // STRICT: All conditions must pass
    if (zombies != 0) {
        printf("  ✗ FAILED - Zombie processes remain (%d found)\n", zombies);
        tests_failed++;
    } else if (shm_count != 0) {
        printf("  ✗ FAILED - IPC resources not cleaned (%d files in /dev/shm)\n", shm_count);
        tests_failed++;
    } else if (shutdown_time_ms > 3000) {
        printf("  ✗ FAILED - Shutdown took too long (%d ms > 3000 ms)\n", shutdown_time_ms);
        tests_failed++;
    } else {
        printf("  ✓ PASSED - Clean shutdown, no zombies, IPC cleaned, fast shutdown\n");
        tests_passed++;
    }
    tests_run++;
    
    pthread_mutex_destroy(&lock);
}

void test_zombie_processes(void) {
    printf("\n[TEST 16] No zombie processes check\n");
    
    sleep(2);
    
    int zombies = check_zombie_processes();
    
    if (zombies < 0) {
        printf("  ✗ FAILED - Cannot check for zombie processes\n");
        tests_failed++;
    } else if (zombies != 0) {
        printf("  Zombie processes: %d\n", zombies);
        printf("  ✗ FAILED - Zombie processes detected (%d found)\n", zombies);
        system("ps aux | grep defunct | grep server");
        tests_failed++;
    } else {
        printf("  Zombie processes: 0\n");
        printf("  ✓ PASSED - No zombies detected\n");
        tests_passed++;
    }
    tests_run++;
}

int main(void) {
    printf("================================================\n");
    printf("Stress Tests\n");
    printf("Tests 13-16: Sustained Load, Memory, Shutdown\n");
    printf("================================================\n");
    
    if (!check_server()) {
        fprintf(stderr, "\n✗ CRITICAL FAILURE: Server not running on %s\n", SERVER_URL);
        fprintf(stderr, "Please start the server first: ./server\n");
        return 1;
    }
    printf("\n✓ Server is running on %s\n", SERVER_URL);
    
    // Tests that don't kill the server
    test_sustained_load();
    
    if (tests_failed > 0) {
        printf("\n✗ STOPPING - Test 13 failed, not proceeding to remaining tests\n");
        goto summary;
    }
    
    test_memory_leaks();
    
    if (tests_failed > 0) {
        printf("\n✗ STOPPING - Test 14 failed, not proceeding to shutdown tests\n");
        goto summary;
    }
    
    // Tests that kill the server (run last)
    printf("\n⚠ WARNING: The following tests will terminate the server\n");
    printf("Proceeding automatically in 3 seconds...\n");
    sleep(3);
    
    test_graceful_shutdown();
    test_zombie_processes();
    
summary:
    printf("\n================================================\n");
    printf("STRESS TEST SUMMARY\n");
    printf("================================================\n");
    printf("Total Tests Run:  %d\n", tests_run);
    printf("Tests Passed:     %d ✓\n", tests_passed);
    printf("Tests Failed:     %d ✗\n", tests_failed);
    
    if (tests_failed == 0 && tests_run == 4) {
        printf("Success Rate:     100.0%% ✓✓✓\n");
        printf("================================================\n");
        printf("\n🎉 ALL STRESS TESTS PASSED!\n");
    } else {
        printf("Success Rate:     %.1f%%\n", 
               tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
        printf("================================================\n");
        printf("\n❌ STRESS TESTS FAILED\n");
    }
    
    if (tests_failed > 0) {
        printf("\n⚠ Server may need to be restarted:\n");
        printf("  ./server\n");
    }
    
    printf("\nFor detailed memory analysis, run:\n");
    printf("  valgrind --leak-check=full --show-leak-kinds=all ./server\n");
    printf("  Then: ab -n 1000 -c 50 http://localhost:8080/\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
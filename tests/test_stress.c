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

// Returns process memory usage in KB
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
    return vmrss;
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
        
        usleep(10000);
    }
    
    pthread_mutex_lock(args->lock);
    *args->success_count += local_success;
    *args->failure_count += local_failure;
    pthread_mutex_unlock(args->lock);
    
    curl_easy_cleanup(curl);
    return NULL;
}

void test_memory_leaks(void) {
    printf("\n[TEST 13] Memory leak detection\n");
    
    pid_t server_pid = get_server_pid();
    if (server_pid < 0) {
        printf("  FAILED - Cannot find server process\n");
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
            printf("  FAILED - %d requests failed in cycle %d\n", failures, cycle + 1);
            tests_failed++;
            tests_run++;
            return;
        }
        
        sleep(2);
        
        long mem = get_process_memory(server_pid);
        if (mem <= 0) {
            printf("  FAILED - Cannot read memory at cycle %d\n", cycle + 1);
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
    
    // Test for memory leak against a 10MB limit (10000 KB)
    if (max_growth < 10000) {
        printf("  PASSED - No memory leak detected (<%ld KB growth)\n", max_growth);
        tests_passed++;
    } else {
        printf("  FAILED - Memory leak detected (%ld KB growth exceeds 10MB limit)\n", max_growth);
        tests_failed++;
    }
    tests_run++;
}

void test_graceful_shutdown(void) {
    printf("\n[TEST 14] Graceful shutdown under load\n");
    
    printf("  This test will terminate the server\n");
    
    pid_t server_pid = get_server_pid();
    if (server_pid < 0) {
        printf("  FAILED - Cannot find server process\n");
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
    
    sleep(2);
    
    // Signal graceful shutdown
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
        usleep(100000);
    }
    
    stop_load = 1;
    pthread_join(load_thread, NULL);
    
    if (!shutdown_clean) {
        printf("  FAILED - Server did not terminate within 5 seconds\n");
        kill(server_pid, SIGKILL);
        tests_failed++;
        tests_run++;
        pthread_mutex_destroy(&lock);
        return;
    }
    
    printf("  Server terminated after %d ms\n", shutdown_time_ms);
    
    sleep(1);
    
    // Check for leftover resources
    int zombies = check_zombie_processes();
    printf("  Zombie processes: %d\n", zombies);
    
    int shm_count = check_shm_cleanup();
    printf("  Leftover /dev/shm files: %d\n", shm_count);
    
    if (zombies != 0) {
        printf("  FAILED - Zombie processes remain (%d found)\n", zombies);
        tests_failed++;
    } else if (shm_count != 0) {
        printf("  FAILED - IPC resources not cleaned (%d files in /dev/shm)\n", shm_count);
        tests_failed++;
    } else if (shutdown_time_ms > 3000) {
        printf("  FAILED - Shutdown took too long (%d ms > 3000 ms)\n", shutdown_time_ms);
        tests_failed++;
    } else {
        printf("  PASSED - Clean shutdown\n");
        tests_passed++;
    }
    tests_run++;
    
    pthread_mutex_destroy(&lock);
}

void test_zombie_processes(void) {
    printf("\n[TEST 15] No zombie processes check\n");
    
    sleep(2);
    
    int zombies = check_zombie_processes();
    
    if (zombies < 0) {
        printf("  FAILED - Cannot check for zombie processes\n");
        tests_failed++;
    } else if (zombies != 0) {
        printf("  Zombie processes: %d\n", zombies);
        printf("  FAILED - Zombie processes detected (%d found)\n", zombies);
        system("ps aux | grep defunct | grep server");
        tests_failed++;
    } else {
        printf("  Zombie processes: 0\n");
        printf("  PASSED - No zombies detected\n");
        tests_passed++;
    }
    tests_run++;
}

int main(void) {
    printf("================================================\n");
    printf("Stress Tests\n");
    printf("Tests 13-15: Memory Leak, Graceful Shutdown, Zombies\n");
    printf("================================================\n");
    
    if (!check_server()) {
        fprintf(stderr, "\nCRITICAL FAILURE: Server not running on %s\n", SERVER_URL);
        fprintf(stderr, "Please start the server first: ./server\n");
        return 1;
    }
    printf("\nServer is running on %s\n", SERVER_URL);
    
    test_memory_leaks();
    
    if (tests_failed > 0) {
        printf("\nSTOPPING - Test 13 failed, not proceeding to shutdown tests\n");
        goto summary;
    }
    
    printf("\nWARNING: The following tests will terminate the server\n");
    printf("Proceeding automatically in 3 seconds...\n");
    sleep(3);
    
    test_graceful_shutdown();
    test_zombie_processes();
    
summary:
    printf("\n================================================\n");
    printf("STRESS TEST SUMMARY\n");
    printf("================================================\n");
    printf("Total Tests Run:  %d\n", tests_run);
    printf("Tests Passed:     %d\n", tests_passed);
    printf("Tests Failed:     %d\n", tests_failed);
    
    if (tests_failed == 0 && tests_run == 3) {
        printf("Success Rate:     100.0%%\n");
        printf("================================================\n");
        printf("\n ALL STRESS TESTS PASSED!\n");
    } else {
        printf("Success Rate:     %.1f%%\n", 
               tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
        printf("================================================\n");
        printf("\n STRESS TESTS FAILED\n");
    }
    
    if (tests_failed > 0) {
        printf("\nServer may need to be restarted:\n");
        printf("  ./server\n");
    }
    
    printf("\nFor detailed memory analysis, run:\n");
    printf("  valgrind --leak-check=full --show-leak-kinds=all ./server\n");
    printf("  Then: ab -n 1000 -c 50 http://localhost:8080/\n");
    
    printf("\nFor manual sustained load testing run test_consitent_load.sh\n");

    return (tests_failed == 0) ? 0 : 1;
}
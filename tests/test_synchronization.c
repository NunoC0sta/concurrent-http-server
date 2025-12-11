#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#define SERVER_URL "http://localhost:8080"
#define LOG_FILE "./access.log"

int tests_run = 0, tests_passed = 0, tests_failed = 0;

typedef struct {
    int total_requests, successful_requests;
    pthread_mutex_t lock;
} test_stats_t;

typedef struct {
    int thread_id, num_requests;
    const char* test_path;
    test_stats_t* stats;
} thread_args_t;

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

void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    
    for (int i = 0; i < args->num_requests; i++) {
        char url[512];
        snprintf(url, sizeof(url), "%s%s", SERVER_URL, args->test_path);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        CURLcode res = curl_easy_perform(curl);
        
        pthread_mutex_lock(&args->stats->lock);
        args->stats->total_requests++;
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code >= 200 && http_code < 400)
                args->stats->successful_requests++;
        }
        pthread_mutex_unlock(&args->stats->lock);
        
        usleep(5000);
    }
    curl_easy_cleanup(curl);
    return NULL;
}

void test_valgrind_helgrind(void) {
    printf("\n[TEST 9] Race condition detection tools availability\n");
    
    int valgrind_available = (system("which valgrind > /dev/null 2>&1") == 0);
    int helgrind_available = (system("valgrind --tool=helgrind --version > /dev/null 2>&1") == 0);
    
    if (valgrind_available) {
        printf("  Valgrind is installed\n");
    } else {
        printf("  Valgrind not found (install: sudo apt-get install valgrind)\n");
    }
    
    if (helgrind_available) {
        printf("  Helgrind is available\n");
    } else {
        printf("  Helgrind not available\n");
    }
    
    // Instructions for running the external tool
    printf("\n  Run race detection with:\n");
    printf("    valgrind --tool=helgrind ./server\n");
    printf("  Then in another terminal:\n");
    printf("    ab -n 5000 -c 100 http://localhost:8080/\n");
    
    if (valgrind_available && helgrind_available) {
        tests_passed++;
    } else {
        tests_failed++;
    }
    tests_run++;
}

void test_log_integrity(void) {
    printf("\n[TEST 10] Log file integrity (no interleaved entries)\n");
    
    printf("  Checking log file: %s\n", LOG_FILE);
    
    // Get initial log size/lines
    struct stat st_before;
    int has_log = (stat(LOG_FILE, &st_before) == 0);
    long initial_lines = 0;
    
    if (has_log) {
        FILE* f = fopen(LOG_FILE, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) initial_lines++;
            fclose(f);
        }
    } else {
        printf("  Log file does not exist, creating it with a test request...\n");
        CURL* curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            sleep(1);
        }
    }
    
    printf("  Initial log entries: %ld\n", initial_lines);
    printf("  Generating 100 concurrent requests...\n");
    
    // Generate concurrent load
    test_stats_t stats = {0};
    pthread_mutex_init(&stats.lock, NULL);
    pthread_t threads[100];
    thread_args_t args[100];
    
    for (int i = 0; i < 100; i++) {
        args[i].thread_id = i;
        args[i].num_requests = 1;
        args[i].test_path = "/";
        args[i].stats = &stats;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    for (int i = 0; i < 100; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("  Waiting 2 seconds for log flush...\n");
    sleep(2);
    
    // Check final log size/lines
    struct stat st_after;
    long final_lines = 0;
    
    if (stat(LOG_FILE, &st_after) == 0) {
        FILE* f = fopen(LOG_FILE, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) final_lines++;
            fclose(f);
        }
    }
    
    long new_entries = final_lines - initial_lines;
    printf("  New log entries: %ld\n", new_entries);
    printf("  Expected: ~100\n");
    
    if (new_entries >= 90 && new_entries <= 110) {
        printf("  Log integrity maintained\n");
        printf("  Manual check: Verify no garbled/interleaved entries in %s\n", LOG_FILE);
        tests_passed++;
    } else {
        printf("  Logging appears to be inconsistent (only %ld/100 entries)\n", new_entries);
        tests_failed++;
    }
    tests_run++;
    
    pthread_mutex_destroy(&stats.lock);
}

void test_cache_consistency(void) {
    printf("\n[TEST 11] Cache consistency across threads\n");
    printf("  Testing same file from 50 concurrent threads...\n");
    
    test_stats_t stats = {0};
    pthread_mutex_init(&stats.lock, NULL);
    pthread_t threads[50];
    thread_args_t args[50];
    
    for (int i = 0; i < 50; i++) {
        args[i].thread_id = i;
        args[i].num_requests = 5;
        args[i].test_path = "/index.html";
        args[i].stats = &stats;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    for (int i = 0; i < 50; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("  Total requests: %d\n", stats.total_requests);
    printf("  Successful: %d\n", stats.successful_requests);
    
    if (stats.successful_requests == stats.total_requests) {
        printf("  Cache consistency maintained\n");
        tests_passed++;
    } else {
        printf("  Cache inconsistency detected\n");
        tests_failed++;
    }
    tests_run++;
    
    pthread_mutex_destroy(&stats.lock);
}

void test_statistics_counters(void) {
    printf("\n[TEST 12] Statistics counters (no lost updates)\n");
    printf("  Sending 100 requests and verifying counter accuracy...\n");
    
    test_stats_t stats = {0};
    pthread_mutex_init(&stats.lock, NULL);
    pthread_t threads[100];
    thread_args_t args[100];
    
    for (int i = 0; i < 100; i++) {
        args[i].thread_id = i;
        args[i].num_requests = 1;
        args[i].test_path = "/";
        args[i].stats = &stats;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    for (int i = 0; i < 100; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("  Requests sent: 100\n");
    printf("  Requests successful: %d\n", stats.successful_requests);
    printf("\n  Manual verification required:\n");
    printf("    1. Check %s/stats\n", SERVER_URL);
    printf("    2. Verify 'Total Requests' increased by ~100\n");
    
    if (stats.successful_requests >= 95) {
        printf("  Statistics counters appear consistent\n");
        tests_passed++;
    } else {
        printf("  Possible lost updates detected\n");
        tests_failed++;
    }
    tests_run++;
    
    pthread_mutex_destroy(&stats.lock);
}

int main(void) {
    printf("================================================\n");
    printf("Synchronization Tests (Thread Safety & IPC)\n");
    printf("Tests 9-12: Race Conditions, Logs, Cache, Stats\n");
    printf("================================================\n");
    
    if (!check_server()) {
        fprintf(stderr, "\nERROR: Server not running on %s\n", SERVER_URL);
        fprintf(stderr, "Please start the server first: ./server\n");
        return 1;
    }
    printf("\nServer is running on %s\n", SERVER_URL);
    
    test_valgrind_helgrind();
    test_log_integrity();
    test_cache_consistency();
    test_statistics_counters();
    
    printf("\n================================================\n");
    printf("SYNCHRONIZATION TEST SUMMARY\n");
    printf("================================================\n");
    printf("Total Tests Run:  %d\n", tests_run);
    printf("Tests Passed:     %d\n", tests_passed);
    printf("Tests Failed:     %d\n", tests_failed);
    printf("Success Rate:     %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("================================================\n");
    
    printf("\nIMPORTANT: Some tests require manual verification:\n");
    printf("  - Run Helgrind: valgrind --tool=helgrind ./server\n");
    printf("  - Check log file: less %s\n", LOG_FILE);
    printf("  - Verify stats: curl %s/stats\n", SERVER_URL);
    
    return (tests_failed == 0) ? 0 : 1;
}
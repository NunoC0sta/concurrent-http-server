#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/time.h>

#define SERVER_URL "http://localhost:8080"

int tests_run = 0, tests_passed = 0, tests_failed = 0;

typedef struct {
    int total_requests, successful_requests, failed_requests;
    long total_response_time_ms;
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
        
        struct timeval start, end;
        gettimeofday(&start, NULL);
        CURLcode res = curl_easy_perform(curl);
        gettimeofday(&end, NULL);
        
        long response_time = (end.tv_sec - start.tv_sec) * 1000 + 
                            (end.tv_usec - start.tv_usec) / 1000;
        
        pthread_mutex_lock(&args->stats->lock);
        args->stats->total_requests++;
        args->stats->total_response_time_ms += response_time;
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code >= 200 && http_code < 400)
                args->stats->successful_requests++;
            else
                args->stats->failed_requests++;
        } else {
            args->stats->failed_requests++;
        }
        pthread_mutex_unlock(&args->stats->lock);
        
        usleep(10000); // 10ms delay between requests
    }
    curl_easy_cleanup(curl);
    return NULL;
}

void run_concurrent_test(const char* test_name, int num_threads, 
                         int requests_per_thread, const char* test_path) {
    printf("\n[%s]\n", test_name);
    printf("  Configuration: %d threads × %d requests = %d total\n", 
           num_threads, requests_per_thread, num_threads * requests_per_thread);
    
    test_stats_t stats = {0};
    pthread_mutex_init(&stats.lock, NULL);
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    thread_args_t* args_array = malloc(num_threads * sizeof(thread_args_t));
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        args_array[i].thread_id = i;
        args_array[i].num_requests = requests_per_thread;
        args_array[i].test_path = test_path;
        args_array[i].stats = &stats;
        pthread_create(&threads[i], NULL, worker_thread, &args_array[i]);
    }
    
    // Wait for all threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    gettimeofday(&end_time, NULL);
    long total_time = (end_time.tv_sec - start_time.tv_sec) * 1000 + 
                     (end_time.tv_usec - start_time.tv_usec) / 1000;
    
    double avg_response = stats.total_requests > 0 ? 
        (double)stats.total_response_time_ms / stats.total_requests : 0.0;
    double rps = total_time > 0 ? (1000.0 * stats.total_requests) / total_time : 0.0;
    
    printf("  Results:\n");
    printf("    Total Requests:  %d\n", stats.total_requests);
    printf("    Successful:      %d (%.1f%%)\n", stats.successful_requests,
           100.0 * stats.successful_requests / stats.total_requests);
    printf("    Failed:          %d (%.1f%%)\n", stats.failed_requests,
           100.0 * stats.failed_requests / stats.total_requests);
    printf("    Total Time:      %ld ms\n", total_time);
    printf("    Avg Response:    %.2f ms\n", avg_response);
    printf("    Requests/sec:    %.2f\n", rps);
    
    if (stats.failed_requests == 0) {
        printf("  ✓ PASSED\n");
        tests_passed++;
    } else {
        printf("  ✗ FAILED (%d dropped connections)\n", stats.failed_requests);
        tests_failed++;
    }
    tests_run++;
    
    pthread_mutex_destroy(&stats.lock);
    free(threads);
    free(args_array);
}

void test_basic_concurrency(void) {
    printf("\n[TEST 5] Basic concurrent load (Apache Bench equivalent)\n");
    printf("  Simulating: ab -n 1000 -c 50\n");
    run_concurrent_test("1000 requests with 50 concurrent clients", 50, 20, "/");
}

void test_no_dropped_connections(void) {
    printf("\n[TEST 6] Verify no dropped connections under load\n");
    run_concurrent_test("100 simultaneous connections", 100, 1, "/");
}

void test_multiple_clients(void) {
    printf("\n[TEST 7] Multiple clients with repeated requests\n");
    run_concurrent_test("50 clients × 10 requests each", 50, 10, "/");
}

void test_statistics_accuracy(void) {
    printf("\n[TEST 8] Statistics accuracy under concurrent load\n");
    printf("  Note: Check /stats endpoint after test completes\n");
    run_concurrent_test("200 concurrent requests for stats verification", 200, 1, "/");
    
    printf("\n  Verify statistics at: %s/stats\n", SERVER_URL);
    printf("  Expected: ~200 requests logged\n");
}

int main(void) {
    printf("================================================\n");
    printf("Concurrency Tests (Multi-threaded Load Testing)\n");
    printf("Tests 5-8: Concurrent Request Handling\n");
    printf("================================================\n");
    
    if (!check_server()) {
        fprintf(stderr, "\nERROR: Server not running on %s\n", SERVER_URL);
        fprintf(stderr, "Please start the server first: ./server\n");
        return 1;
    }
    printf("\n✓ Server is running on %s\n", SERVER_URL);
    
    test_basic_concurrency();
    test_no_dropped_connections();
    test_multiple_clients();
    test_statistics_accuracy();
    
    printf("\n================================================\n");
    printf("CONCURRENCY TEST SUMMARY\n");
    printf("================================================\n");
    printf("Total Tests Run:  %d\n", tests_run);
    printf("Tests Passed:     %d ✓\n", tests_passed);
    printf("Tests Failed:     %d ✗\n", tests_failed);
    printf("Success Rate:     %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("================================================\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
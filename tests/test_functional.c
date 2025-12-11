#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>

#define SERVER_URL "http://localhost:8080"

int tests_run = 0, tests_passed = 0, tests_failed = 0;

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

int file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

long get_http_status(const char* url, int use_head) {
    CURL* curl = curl_easy_init();
    if (!curl) return 0;
    long status_code = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    if (use_head) curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    curl_easy_cleanup(curl);
    return status_code;
}

char* get_content_type(const char* url) {
    static char content_type[256];
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_perform(curl);
    
    char* ct = NULL;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
    if (ct) strncpy(content_type, ct, sizeof(content_type) - 1);
    
    curl_easy_cleanup(curl);
    return ct ? content_type : NULL;
}

void test_file_types(void) {
    printf("\n[TEST 1] GET requests for various file types\n");
    
    // Test files that we know exist based on your project structure
    struct {
        const char* file;
        const char* local_path;
    } files[] = {
        {"index.html", "www/index.html"},
        {"test.txt", "www/test.txt"},
        {"about.html", "www/about.html"}
    };
    
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        // Check if file exists locally first
        if (!file_exists(files[i].local_path)) {
            printf("  ⊘ %s (skipped - file doesn't exist in www/)\n", files[i].file);
            continue;
        }
        
        char url[512];
        snprintf(url, sizeof(url), "%s/%s", SERVER_URL, files[i].file);
        long status = get_http_status(url, 0);
        
        if (status == 200) {
            printf("  ✓ %s (HTTP %ld)\n", files[i].file, status);
            tests_passed++;
        } else {
            printf("  ✗ %s (HTTP %ld - expected 200)\n", files[i].file, status);
            tests_failed++;
        }
        tests_run++;
    }
}

void test_status_codes(void) {
    printf("\n[TEST 2] Verify correct HTTP status codes\n");
    
    // Test 200 OK
    long status = get_http_status(SERVER_URL "/", 0);
    if (status == 200) {
        printf("  ✓ GET / = %ld (OK)\n", status);
        tests_passed++;
    } else {
        printf("  ✗ GET / = %ld (expected 200)\n", status);
        tests_failed++;
    }
    tests_run++;
    
    // Test 404 Not Found
    status = get_http_status(SERVER_URL "/nonexistent_file_12345.html", 0);
    if (status == 404) {
        printf("  ✓ GET /nonexistent_file_12345.html = %ld (Not Found)\n", status);
        tests_passed++;
    } else {
        printf("  ✗ GET /nonexistent_file_12345.html = %ld (expected 404)\n", status);
        tests_failed++;
    }
    tests_run++;
    
    // Test HEAD method
    status = get_http_status(SERVER_URL "/", 1);
    if (status == 200) {
        printf("  ✓ HEAD / = %ld (OK)\n", status);
        tests_passed++;
    } else {
        printf("  ✗ HEAD / = %ld (expected 200)\n", status);
        tests_failed++;
    }
    tests_run++;
}

void test_directory_index(void) {
    printf("\n[TEST 3] Directory index serving\n");
    
    long status = get_http_status(SERVER_URL "/", 0);
    if (status == 200) {
        printf("  ✓ Index automatically served for / (HTTP %ld)\n", status);
        tests_passed++;
    } else {
        printf("  ✗ Index not served (HTTP %ld)\n", status);
        tests_failed++;
    }
    tests_run++;
}

void test_content_types(void) {
    printf("\n[TEST 4] Verify correct Content-Type headers\n");
    
    // Only test files that exist
    struct {
        const char* file;
        const char* local_path;
        const char* expected_type;
    } tests[] = {
        {"/index.html", "www/index.html", "text/html"},
        {"/test.txt", "www/test.txt", "text/plain"}
    };
    
    int skipped = 0;
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        if (!file_exists(tests[i].local_path)) {
            printf("  ⊘ %s (skipped - file doesn't exist)\n", tests[i].file);
            skipped++;
            continue;
        }
        
        char url[512];
        snprintf(url, sizeof(url), "%s%s", SERVER_URL, tests[i].file);
        char* ct = get_content_type(url);
        
        if (ct && strstr(ct, tests[i].expected_type)) {
            printf("  ✓ %s → %s\n", tests[i].file, ct);
            tests_passed++;
        } else {
            printf("  ✗ %s → %s (expected %s)\n", tests[i].file, 
                   ct ? ct : "NULL", tests[i].expected_type);
            tests_failed++;
        }
        tests_run++;
    }
    
    if (skipped > 0) {
        printf("  ℹ %d file(s) skipped (not present in www/)\n", skipped);
    }
}

int main(void) {
    printf("================================================\n");
    printf("Functional Tests (HTTP Protocol & File Serving)\n");
    printf("Tests 1-4: Basic HTTP Functionality\n");
    printf("================================================\n");
    
    if (!check_server()) {
        fprintf(stderr, "\nERROR: Server not running on %s\n", SERVER_URL);
        fprintf(stderr, "Please start the server first: ./server\n");
        return 1;
    }
    printf("\n✓ Server is running on %s\n", SERVER_URL);
    
    test_file_types();
    test_status_codes();
    test_directory_index();
    test_content_types();
    
    printf("\n================================================\n");
    printf("FUNCTIONAL TEST SUMMARY\n");
    printf("================================================\n");
    printf("Total Tests Run:  %d\n", tests_run);
    printf("Tests Passed:     %d ✓\n", tests_passed);
    printf("Tests Failed:     %d ✗\n", tests_failed);
    
    if (tests_run > 0) {
        printf("Success Rate:     %.1f%%\n", (100.0 * tests_passed / tests_run));
    }
    
    printf("================================================\n");
    
    if (tests_failed > 0) {
        printf("\nSome tests failed. Common issues:\n");
        printf("  - Missing files in www/ directory\n");
        printf("  - Incorrect MIME type mapping in http.c\n");
        printf("  - DOCUMENT_ROOT misconfiguration in server.conf\n");
    }
    
    return (tests_failed == 0) ? 0 : 1;
}
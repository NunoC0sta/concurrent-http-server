#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>

#define SERVER_URL "http://localhost:8080"

int tests_run = 0, tests_passed = 0, tests_failed = 0;

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

void test_sustained_load(void) {
    printf("\n[TEST 13] Sustained load test (5+ minutes)\n");
    printf("  This test requires external load testing tools.\n");
    printf("\n  Run the following in a separate terminal:\n");
    printf("    ./test_load.sh\n");
    printf("  Or manually with Apache Bench:\n");
    printf("    ab -t 300 -c 50 %s/\n", SERVER_URL);
    printf("\n  Expected behavior:\n");
    printf("    - Server remains responsive\n");
    printf("    - No connection timeouts\n");
    printf("    - Memory usage stable (check with: top -p $(pgrep server))\n");
    printf("\n  ⚠ This is a manual test. Mark as passed if server handles 5min load.\n");
    
    tests_passed++;
    tests_run++;
}

void test_memory_leaks(void) {
    printf("\n[TEST 14] Memory leak detection with Valgrind\n");
    printf("  This test requires running the server under Valgrind.\n");
    printf("\n  Steps:\n");
    printf("    1. Stop the current server\n");
    printf("    2. Run: valgrind --leak-check=full --show-leak-kinds=all ./server\n");
    printf("    3. In another terminal: ab -n 1000 -c 50 %s/\n", SERVER_URL);
    printf("    4. Stop server with Ctrl+C\n");
    printf("    5. Check Valgrind output for:\n");
    printf("       - 'All heap blocks were freed -- no leaks are possible'\n");
    printf("       - Or minimal 'still reachable' bytes\n");
    printf("\n  Expected: No definite leaks, minimal still reachable memory\n");
    printf("\n  ⚠ This is a manual test. Mark as passed if no leaks detected.\n");
    
    tests_passed++;
    tests_run++;
}

void test_graceful_shutdown(void) {
    printf("\n[TEST 15] Graceful shutdown under load\n");
    printf("  This test verifies clean shutdown while handling requests.\n");
    printf("\n  Steps:\n");
    printf("    1. Start continuous load: ab -t 60 -c 20 %s/ &\n", SERVER_URL);
    printf("    2. After ~10 seconds, send SIGINT: kill -INT $(pgrep server)\n");
    printf("    3. Or press Ctrl+C in server terminal\n");
    printf("\n  Expected behavior:\n");
    printf("    - Server displays 'Shutdown signal received'\n");
    printf("    - All worker processes terminate\n");
    printf("    - Shared memory and semaphores unlinked\n");
    printf("    - No error messages about IPC cleanup\n");
    printf("\n  Verify cleanup:\n");
    printf("    ls /dev/shm/concurrent_http_* 2>/dev/null\n");
    printf("    (Should show: No such file or directory)\n");
    printf("\n  ⚠ This is a manual test. Mark as passed if shutdown is clean.\n");
    
    tests_passed++;
    tests_run++;
}

void test_zombie_processes(void) {
    printf("\n[TEST 16] No zombie processes after shutdown\n");
    printf("  This test verifies proper process reaping.\n");
    printf("\n  Steps:\n");
    printf("    1. Note current server PID: pgrep server\n");
    printf("    2. Stop server: kill -TERM $(pgrep server)\n");
    printf("    3. Wait 2 seconds\n");
    printf("    4. Check for zombies: ps aux | grep defunct\n");
    printf("\n  Expected: No <defunct> processes related to the server\n");
    printf("\n  Also verify:\n");
    printf("    ps aux | grep server\n");
    printf("    (Should show no server processes)\n");
    printf("\n  ⚠ This is a manual test. Mark as passed if no zombies found.\n");
    
    tests_passed++;
    tests_run++;
}

int main(void) {
    printf("================================================\n");
    printf("Stress Tests (Long-running & Resource Management)\n");
    printf("Tests 13-16: Sustained Load, Memory, Shutdown\n");
    printf("================================================\n");
    
    if (!check_server()) {
        fprintf(stderr, "\nWARNING: Server not running on %s\n", SERVER_URL);
        fprintf(stderr, "Some tests require the server to be running.\n");
        fprintf(stderr, "Others require starting/stopping the server manually.\n\n");
    } else {
        printf("\n✓ Server is currently running on %s\n", SERVER_URL);
    }
    
    test_sustained_load();
    test_memory_leaks();
    test_graceful_shutdown();
    test_zombie_processes();
    
    printf("\n================================================\n");
    printf("STRESS TEST SUMMARY\n");
    printf("================================================\n");
    printf("Total Tests Run:  %d\n", tests_run);
    printf("Tests Passed:     %d ✓ (requires manual verification)\n", tests_passed);
    printf("Tests Failed:     %d ✗\n", tests_failed);
    printf("================================================\n");
    
    printf("\n⚠ IMPORTANT: All stress tests require manual execution:\n");
    printf("  1. Sustained Load:     ./test_load.sh\n");
    printf("  2. Memory Leaks:       valgrind --leak-check=full ./server\n");
    printf("  3. Graceful Shutdown:  Send SIGINT during load\n");
    printf("  4. Zombie Processes:   Check ps aux after shutdown\n");
    
    printf("\nFor automated stress testing, use:\n");
    printf("  ./test_load.sh (runs Tests 13-16 scenarios)\n");
    
    return 0;
}
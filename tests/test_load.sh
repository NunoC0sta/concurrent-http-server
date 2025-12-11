#!/bin/bash

# Load testing script for concurrent HTTP server

set -e

SERVER_HOST="localhost"
SERVER_PORT="8080"
BASE_URL="http://$SERVER_HOST:$SERVER_PORT"
RESULTS_FILE="load_test_results.txt"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

check_server() {
    if ! curl -s "$BASE_URL" > /dev/null 2>&1; then
        echo -e "${RED}ERROR: Server is not running on $BASE_URL${NC}"
        exit 1
    fi
}

run_load_test() {
    local num_requests=$1
    local concurrent=$2
    local test_path=$3
    
    echo -e "${YELLOW}Testing with $num_requests requests ($concurrent concurrent)${NC}"
    
    if command -v ab &> /dev/null; then
        ab -n "$num_requests" -c "$concurrent" -q "$BASE_URL$test_path" 2>&1 | tee -a "$RESULTS_FILE"
    else
        echo "Apache Bench (ab) not found, using curl loop instead..."
        local start_time=$(date +%s%3N)
        
        for ((i=0; i<num_requests; i++)); do
            curl -s "$BASE_URL$test_path" > /dev/null &
            
            # Concurrency control for curl loop
            if (( (i + 1) % concurrent == 0 )); then
                wait
            fi
        done
        
        wait
        local end_time=$(date +%s%3N)
        local elapsed=$((end_time - start_time))
        
        echo "Completed $num_requests requests in ${elapsed}ms"
    fi
    
    echo "---" >> "$RESULTS_FILE"
}

main() {
    echo -e "${GREEN}=== Concurrent HTTP Server Load Test ===${NC}\n"
    
    check_server
    echo -e "${GREEN}Server is running${NC}\n"
    
    > "$RESULTS_FILE"
    
    echo "=== Load Test Results ===" > "$RESULTS_FILE"
    echo "Test Date: $(date)" >> "$RESULTS_FILE"
    echo "" >> "$RESULTS_FILE"
    
    echo -e "${YELLOW}[Test 1] Light Load${NC}"
    run_load_test 10 2 "/"
    sleep 1
    
    echo -e "${YELLOW}[Test 2] Moderate Load${NC}"
    run_load_test 50 5 "/"
    sleep 1
    
    echo -e "${YELLOW}[Test 3] Heavy Load${NC}"
    run_load_test 100 10 "/"
    sleep 1
    
    echo -e "${YELLOW}[Test 4] Very Heavy Load${NC}"
    run_load_test 200 20 "/"
    sleep 1
    
    echo -e "${YELLOW}[Test 5] Mixed Request Types${NC}"
    echo "GET requests:" >> "$RESULTS_FILE"
    run_load_test 30 5 "/"
    
    echo "HEAD requests:" >> "$RESULTS_FILE"
    for ((i=0; i<30; i++)); do
        curl -s -I "$BASE_URL/" > /dev/null &
        if (( (i + 1) % 5 == 0 )); then
            wait
        fi
    done
    wait
    
    sleep 1
    
    if curl -s "$BASE_URL/index.html" > /dev/null 2>&1; then
        echo -e "${YELLOW}[Test 6] File Request Load${NC}"
        run_load_test 30 5 "/index.html"
        sleep 1
    fi
    
    echo -e "${YELLOW}[Test 7] 404 Error Load${NC}"
    run_load_test 20 5 "/nonexistent.html"
    sleep 1
    
    echo -e "\n${GREEN}=== Test Results Summary ===${NC}"
    cat "$RESULTS_FILE"
    
    echo -e "\n${GREEN}Load testing completed${NC}"
    echo "Full results saved to: $RESULTS_FILE"
}

main "$@"
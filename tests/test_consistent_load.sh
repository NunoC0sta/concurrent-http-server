#!/bin/bash
# real_sustained_load.sh

URL="http://localhost:8080/"
DURATION=300
CONCURRENT=50
BATCH_SIZE=1000

echo "=== Sustained Load Test ==="
echo "Duration: ${DURATION}s"
echo "Concurrency: ${CONCURRENT}"
echo "Target: ${URL}"
echo ""

START=$(date +%s)
END_TIME=$((${START} + ${DURATION}))

TOTAL_REQUESTS=0
TOTAL_SUCCESS=0
TOTAL_FAILED=0

while [ $(date +%s) -lt ${END_TIME} ]; do
    ELAPSED=$(($(date +%s) - ${START}))
    echo -ne "\rElapsed: ${ELAPSED}s / ${DURATION}s | Requests: ${TOTAL_REQUESTS}"
    
    # Run a batch of requests
    OUTPUT=$(ab -n ${BATCH_SIZE} -c ${CONCURRENT} -q ${URL} 2>&1)
    
    # Parse results
    COMPLETE=$(echo "$OUTPUT" | grep "Complete requests:" | awk '{print $3}')
    FAILED=$(echo "$OUTPUT" | grep "Failed requests:" | awk '{print $3}')
    
    TOTAL_REQUESTS=$((${TOTAL_REQUESTS} + ${COMPLETE:-0}))
    TOTAL_SUCCESS=$((${TOTAL_SUCCESS} + ${COMPLETE:-0} - ${FAILED:-0}))
    TOTAL_FAILED=$((${TOTAL_FAILED} + ${FAILED:-0}))
done

echo ""
echo ""
echo "=== RESULTS ==="
echo "Total Requests: ${TOTAL_REQUESTS}"
echo "Successful: ${TOTAL_SUCCESS}"
echo "Failed: ${TOTAL_FAILED}"
echo "Success Rate: $(awk "BEGIN {printf \"%.2f\", (${TOTAL_SUCCESS}*100.0/${TOTAL_REQUESTS})}")%"
echo "Requests/sec: $(awk "BEGIN {printf \"%.2f\", (${TOTAL_REQUESTS}/${DURATION})}")"
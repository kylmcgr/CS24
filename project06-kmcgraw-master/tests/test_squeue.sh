#!/bin/bash

cd `dirname "${0}"`

QUEUE_TESTS=(squeue_single_fill squeue_push_pop)

for test in ${QUEUE_TESTS[@]}; do
    echo -e "Running ${test}"
    if ! ../bin/${test}; then
        echo "FAILED test ${test}"
        exit 1
    fi
    echo -e "PASSED test ${test}"
done

echo -e "ALL SINGLE-THREADED QUEUE TESTS PASS!"

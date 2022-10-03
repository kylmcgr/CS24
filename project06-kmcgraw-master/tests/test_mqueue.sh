#!/bin/bash

cd `dirname "${0}"`

echo -e "Running mqueue_push_pop"
if ! diff -u correct_integers.txt <(../bin/mqueue_push_pop | sort -n); then
    echo -e "FAILED test mqueue_push_pop"
    exit 1
fi
echo -e "PASSED test mqueue_push_pop"

echo -e "Running mqueue_test_empty"
if ! ../bin/mqueue_test_empty; then
    echo -e "FAILED test mqueue_test_empty"
    exit 1
fi
echo -e "PASSED test mqueue_test_empty"

echo -e "ALL MULTI-THREADED QUEUE TESTS PASS!"

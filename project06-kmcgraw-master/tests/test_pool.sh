#!/bin/bash

cd `dirname "${0}"`

PRIMES_THREADS=(1 2 4 8 16 32 64)
PRIMES_TESTS=(multiple_pools repeat_drain periodic_work)
EXPECTED_RECURSIVE_WORK=3125
SLEEPERS=10
SLEEPER_THREADS=$(seq 1 ${SLEEPERS})

for threads in ${PRIMES_THREADS[@]}; do
    echo -e "Runnning prime_printer with ${threads} threads"
    if ! diff -u correct_primes.txt <(../bin/prime_printer ${threads} | sort -n); then
        echo -e "FAILED test prime_printer with ${threads} threads"
        exit 1
    fi
    echo "PASSED test prime_printer with ${threads} threads"
done

for test in ${PRIMES_TESTS[@]}; do
    echo -e "Running ${test}"
    if ! diff -u correct_primes.txt <(../bin/${test} | sort -n); then
        echo -e "FAILED test ${test}"
        exit 1
    fi
    echo -e "PASSED test ${test}"
done

echo -e "Running recursive_add_work"
RECURSIVE_WORK_COUNT=$(../bin/recursive_add_work | wc -l)
if ((${RECURSIVE_WORK_COUNT} != ${EXPECTED_RECURSIVE_WORK})); then
    echo -e "FAILED test recursive_add_work!"
    echo -e "Expected ${EXPECTED_RECURSIVE_WORK} prints, but found ${RECURSIVE_WORK_COUNT}."
    exit 1
fi
echo -e "PASSED test recursive_add_work"

TIMEFORMAT=%0R
for threads in ${SLEEPER_THREADS[@]}; do
    echo -e "Running sleepers with ${threads} threads"
    ((EXPECTED_TIME = (${SLEEPERS} + ${threads} - 1) / ${threads}))
    ACTUAL_TIME=$({ time ../bin/sleepers ${threads} ; } 2>&1)
    if ((${ACTUAL_TIME} != ${EXPECTED_TIME})); then
        echo -e "FAILED test sleepers with ${threads} threads"
        echo -e "Expected to sleep ${EXPECTED_TIME} seconds, but slept ${ACTUAL_TIME} seconds."
        exit 1
    fi
    echo -e "PASSED test sleepers with ${threads} threads"
done

echo -e "ALL THREADPOOL TESTS PASS!"

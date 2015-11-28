#!/bin/sh

HW_THREAD_IDS=( $(kstat cpu_info | grep -E 'instance' | awk '{print $4}') )
CORE_IDS=( $(kstat cpu_info | grep -E 'core_id' | awk '{print $2}') )
last_id=${CORE_IDS[0]}
while [[ $i -lt ${#HW_THREAD_IDS[@]} ]]; do
    if [[ $last_id -ne ${CORE_IDS[$i]} ]]; then
        last_id=${CORE_IDS[$i]}
        echo
    fi
    echo -n "${HW_THREAD_IDS[$i]} "
    (( i++ ))
done
echo


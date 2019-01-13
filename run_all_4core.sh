#!/bin/bash

BINARY_NAME=${1}
NUM_MIXES="$(cat ./sim_list/4core_workloads.txt | wc -l)"

for((i=1;i<=NUM_MIXES;i=i+1))
do
    sleep 2
    array=($(pidof $BINARY_NAME))
    echo ${#array[@]}
    while [ ${#array[@]} -ge 10 ]
    do
        sleep 60
        array=($(pidof $BINARY_NAME))
    done
    
    ./run_4core.sh $BINARY_NAME 20 80 $i &
done

wait
exit 0


#!/bin/bash

BINARY_NAME=${1}

for trace in cassandra cloud9 em3d streaming zeus_40cl
do
    sleep 2
    array=($(pidof $BINARY_NAME))
    echo ${#array[@]}
    while [ ${#array[@]} -ge 10 ]
    do
        sleep 60
        array=($(pidof $BINARY_NAME))
    done
    
    ./run_cloud.sh $BINARY_NAME 20 80 ${trace} -cloudsuite &
done

wait
exit 0


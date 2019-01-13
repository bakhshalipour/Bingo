#!/bin/bash

cd bin
bins=$(ls -q *)
cd ..

for BINARY_NAME in $bins
do
    x=$(ls results_4core_cloud/*$BINARY_NAME* | wc -l)
    if [ $x -eq 0 ]
    then
        ./run_all_cloud.sh $BINARY_NAME &
        echo $BINARY_NAME
    fi
done

wait
exit 0


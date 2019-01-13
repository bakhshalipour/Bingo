#!/bin/bash

LIST=$(ls prefetcher/*.llc_pref | cut -d '/' -f2 | cut -d '.' -f1)

for prefetcher in $LIST
do
    ./build_champsim.sh perceptron-large no $prefetcher lru 4
done


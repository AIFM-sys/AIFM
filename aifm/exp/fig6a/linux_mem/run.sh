#!/bin/bash

source ../../../shared.sh

num_threads_arr=(1 2 3 4 5 6 7 8 9 10 11)

sudo pkill -9 main

for num_threads in ${num_threads_arr[@]}
do
    sed "s/constexpr static uint32_t kNumMutatorThreads.*/constexpr static uint32_t kNumMutatorThreads = $num_threads;/g" main.cpp -i
    make clean
    make -j
    rerun_local_iokerneld_args simple 1,2,3,4,5,6,7,8,9,11,12,13,14,15
    rerun_mem_server
    run_program ./main | grep "mops\|tail" 1>log.$num_threads 2>&1
done

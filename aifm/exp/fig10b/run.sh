#!/bin/bash

source ../../shared.sh

num_mutators_arr=( 20 40 60 80 100 120 140 160 180 200 220 240 260 280 300 320 340 360 380 400 )

sudo pkill -9 main
for num_mutators in ${num_mutators_arr[@]}
do
    sed "s/constexpr static uint32_t kNumMutatorThreads.*/constexpr static uint32_t kNumMutatorThreads = $num_mutators;/g" main.cpp -i
    make clean
    make -j
    rerun_local_iokerneld
    rerun_mem_server
    run_program ./main 1>log.$num_mutators 2>&1
done
kill_local_iokerneld

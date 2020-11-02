#!/bin/bash

source ../../shared.sh

cache_sizes_arr=( 400 600 800 1000 1200 1400 1600 1800 2000 2200 2400 2600 2800 3100 3150 3200 3250 3300 3350 3400 3600 3800 4000 4200 4400 4600 4800 5000 )

sudo pkill -9 main
for cache_size in ${cache_sizes_arr[@]}
do
    sed "s/constexpr static uint64_t kCacheSize.*/constexpr static uint64_t kCacheSize = $cache_size * Region::kSize;/g" main.cpp -i
    make clean
    make -j
    rerun_local_iokerneld
    rerun_mem_server
    run_program ./main 1>log.$cache_size 2>&1
done
kill_local_iokerneld

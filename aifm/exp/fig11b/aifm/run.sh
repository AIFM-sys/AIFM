#!/bin/bash

source ../../../shared.sh

local_rams=( 256 512 1024 2048 3072 4096 5120 6144 7168 8192 9216 10240 11264 12288 13312 14336 15360 16384 17408 18432 )

rm log.*
sudo pkill -9 main
for local_rams in "${local_rams[@]}" 
do
    sed "s/constexpr uint64_t kCacheSize = .*/constexpr uint64_t kCacheSize = $local_rams \* Region::kSize;/g" main.cpp -i
    make clean
    make -j
    rerun_local_iokerneld_noht
    rerun_mem_server
    run_program_noht ./main 1>log.$local_rams 2>&1 &
    ( tail -f -n0 log.$local_rams & ) | grep -q "Force existing..."
    sudo pkill -9 main
done
kill_local_iokerneld

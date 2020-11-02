#!/bin/bash

source ../../../shared.sh

ns_array=( 0 300 600 900 1200 1600 1800 2000 3000 4000 5000 6000 7000 8000 9000 10000 11000 12000 )

rm log.*
sudo pkill -9 main
for delay_ns in "${ns_array[@]}"
do
    make clean
    make CXXFLAGS="-DDELAY_NS_PER_ITER=$delay_ns" -j
    rerun_local_iokerneld_noht
    rerun_mem_server
    run_program_noht ./main 1>log.$delay_ns 2>&1    
done
kill_local_iokerneld

#!/bin/bash

source ../../shared.sh

sudo pkill -9 main
make clean
make -j CXXFLAGS="-DDISABLE_PRIORITIZING -DMONITOR_FREE_MEM_RATIO"

rerun_local_iokerneld
rerun_mem_server
echo > log.no_prioritizing
run_program ./main 1>log.no_prioritizing 2>&1 &
( tail -f -n0 log.no_prioritizing & ) | grep -q "runs out of space"

sudo pkill -9 main
kill_local_iokerneld
kill_mem_server
make clean

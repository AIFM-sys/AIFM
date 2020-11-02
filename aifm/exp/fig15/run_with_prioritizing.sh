#!/bin/bash

source ../../shared.sh

sudo pkill -9 main
make clean
make -j CXXFLAGS="-DMONITOR_FREE_MEM_RATIO"

rerun_local_iokerneld
rerun_mem_server
echo > log.with_prioritizing
run_program ./main 1>log.with_prioritizing 2>&1

sudo pkill -9 main
kill_local_iokerneld
kill_mem_server
make clean

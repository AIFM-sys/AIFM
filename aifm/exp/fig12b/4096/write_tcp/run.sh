#!/bin/bash

source ../../../../shared.sh

sudo pkill -9 main
make clean
make -j CXXFLAGS="-DMONITOR_READ_OBJECT_CYCLES"
rerun_local_iokerneld_noht
rerun_mem_server
run_program_noht ./main
kill_local_iokerneld

#!/bin/bash

source ../../../../shared.sh

sudo pkill -9 main
make clean
make -j
rerun_local_iokerneld_noht
rerun_mem_server
run_program_noht ./main
kill_local_iokerneld

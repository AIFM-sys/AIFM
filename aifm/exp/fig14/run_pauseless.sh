#!/bin/bash

source ../../shared.sh

sudo pkill -9 main
make clean
make -j

rerun_local_iokerneld
rerun_mem_server
run_program ./main 1>log.pauseless 2>&1

sudo pkill -9 main
kill_local_iokerneld
kill_mem_server
make clean

#!/bin/bash

source ../../../../shared.sh

sudo pkill -9 main
make clean
make -j
rerun_local_iokerneld
sudo sh -c "echo always > /sys/kernel/mm/transparent_hugepage/enabled"
run_program ./main
sudo sh -c "echo madvise > /sys/kernel/mm/transparent_hugepage/enabled"
kill_local_iokerneld

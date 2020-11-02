#!/bin/bash

sudo sh -c "echo '+memory' > /sys/fs/cgroup/unified/cgroup.subtree_control"
sudo sh -c "mkdir /sys/fs/cgroup/unified/bench"
sudo sh -c "echo $$ > /sys/fs/cgroup/unified/bench/cgroup.procs"

cache_sizes=(31 29 27 25 23 21 19 17 15 13 11 9 7 5 3 1)

sudo pkill -9 main

log_folder=`pwd`
cd ../../../DataFrame/original/
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-9 ..
make -j

for cache_size in ${cache_sizes[@]}
do
    mem_bytes_limit=`echo $(($(($(($cache_size * 1024))*1024))*1024))`
    sudo sh -c "echo $mem_bytes_limit > /sys/fs/cgroup/unified/bench/memory.high"
    sudo taskset -c 1 ./bin/main > $log_folder/log.$cache_size
done

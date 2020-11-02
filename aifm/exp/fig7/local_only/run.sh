#!/bin/bash

sudo pkill -9 main

log_folder=`pwd`
cd ../../../DataFrame/original/
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-9 ..
make -j

sudo taskset -c 1 ./bin/main > $log_folder/log

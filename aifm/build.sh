#!/bin/bash

# AIFM Core.
make clean
make -j$(nproc) || { echo 'Failed to build AIFM Core.'; exit 1; }

# Far-Mem Snappy.
cd snappy
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release .. || { echo 'Failed to build Snappy.'; exit 1; }
make -j
cd ..

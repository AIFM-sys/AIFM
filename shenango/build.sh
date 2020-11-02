#!/bin/bash

# Ksched
cd ksched
make clean
make || { echo 'Failed to build ksched.'; exit 1; }
cd ..

# DPDK
./dpdk.sh || { echo 'Failed to build DPDK.'; exit 1; }

# RDMA
./rdma-core.sh || { echo 'Failed to build RDMA core.'; exit 1; }

# Shenango Core
make clean
make -j$(nproc) || { echo 'Failed to build Shenango core.'; exit 1; }

# Bindings
cd bindings/cc
make clean
make -j$(nproc) || { echo 'Failed to build Shenango bindings.'; exit 1; }
cd ../..

# Setup
sudo ./scripts/setup_machine.sh || { echo 'Failed to setup Shenango.'; exit 1; }

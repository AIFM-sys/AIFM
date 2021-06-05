# AIFM
![Status](https://img.shields.io/badge/Version-Experimental-green.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

AIFM stands for Application-Integrated Far Memory. It provides a simple, general, and high-performance mechanism for users to adapt ordinary memory-intensive applications to far memory. Different from existing paging-based systems, AIFM exposes far memory as far-memory pointers and containers in the language level. AIFM's API allows its runtime to accurately capture application semantics, therefore making intelligent decisions on data placement and movement.

Currently, AIFM supports C++ and TCP-enabled remote server memory.

- [AIFM](#aifm)
  * [Paper](#paper)
  * [Supported Platform](#supported-platform)
  * [Build Instructions](#build-instructions)
    + [Configure Cloudlab Instances](#configure-cloudlab-instances)
    + [Install Dependencies (on all nodes)](#install-dependencies-on-all-nodes)
    + [Build Shenango and AIFM (on all nodes)](#build-shenango-and-aifm-on-all-nodes)
    + [Setup Shenango (on all nodes)](#setup-shenango-on-all-nodes)
    + [Configure AIFM (only on the compute node)](#configure-aifm-only-on-the-compute-node)
  * [Run AIFM Tests](#run-aifm-tests)
  * [Reproduce Experiment Results](#reproduce-experiment-results)
  * [Repo Structure](#repo-structure)
  * [Contact](#contact)

## Paper
* [AIFM: High-Performance, Application-Integrated Far Memory](https://www.usenix.org/conference/osdi20/presentation/ruan)<br>
Zhenyuan Ruan, Malte Schwarzkopf, Marcos Aguilera, Adam Belay<br>
The 14th USENIX Symposium on Operating Systems Design and Implementation (OSDI ‘20)

## Supported Platform

We have adapted AIFM to [Cloudlab](https://www.cloudlab.us/) for public access.

## Build Instructions
### Configure Cloudlab Instances

1) Apply a Cloudlab account if you do not have one.

2) Now you have logged into Cloublab console. Click `Experiments`|-->`Create Experiment Profile`. Upload `cloudlab.profile` provided in this repo root.

3) Create a two-node instance using the profile. 

### Install Dependencies (on all nodes)
Now you have logged into your Cloudlab instances. You have to install the necessary dependencies in order to build AIFM. Note you have to do run those steps on all Cloudlab nodes you have created.

1) Update package database and Linux kernel version.
```
sudo apt-get update
echo Y | sudo apt-get install linux-headers-5.0.0-20 linux-headers-5.0.0-20-generic linux-hwe-edge-tools-5.0.0-20 linux-image-5.0.0-20-generic linux-modules-5.0.0-20-generic linux-tools-5.0.0-20-generic
sudo reboot
```

2) Install Mellanox OFED.
```
wget "http://content.mellanox.com/ofed/MLNX_OFED-4.6-1.0.1.1/MLNX_OFED_LINUX-4.6-1.0.1.1-ubuntu18.04-x86_64.tgz"
tar xvf MLNX_OFED_LINUX-4.6-1.0.1.1-ubuntu18.04-x86_64.tgz
cd MLNX_OFED_LINUX-4.6-1.0.1.1-ubuntu18.04-x86_64
sudo ./mlnxofedinstall --add-kernel-support --dpdk --upstream-libs # it's fine to see 'Failed to install libibverbs-dev DEB'
sudo /etc/init.d/openibd restart
```

3) Install libraries and tools.
```
echo Y | sudo apt-get --fix-broken install
echo Y | sudo apt-get install libnuma-dev libmnl-dev libnl-3-dev libnl-route-3-dev
echo Y | sudo apt-get install libcrypto++-dev libcrypto++-doc libcrypto++-utils
echo Y | sudo apt-get install software-properties-common
echo Y | sudo apt-get install gcc-9 g++-9 python-pip
echo Y | sudo add-apt-repository ppa:ubuntu-toolchain-r/test
echo Y | sudo apt-get purge cmake
sudo pip install cmake
```

### Build Shenango and AIFM (on all nodes)
For all nodes, clone our github repo in a same path, say, your home directory. 

AIFM relies on Shenango's threading and TCP runtime. The `build_all.sh` script in repo root compiles both Shenango and AIFM automatically.
```
./build_all.sh
```

### Setup Shenango (on all nodes)
After rebooting machines, you have to rerun the script to setup Shenango.
```
sudo ./scripts/setup_machine.sh
```

### Configure AIFM (only on the compute node)
So far you have built AIFM on both nodes. One node is used as the compute node to run applications, while the other node is used as the remote memory node. Now edit `aifm/configs/ssh` in the compute node; change `MEM_SERVER_SSH_IP` to the IP of the remote memory node (eno49 inet in `ifconfig`), and `MEM_SERVER_SSH_USER` to your ssh username. Please make sure the compute node can ssh the remote memory node successfully without password.

## Run AIFM Tests
Now you are able to run AIFM programs. `aifm/test` contains a bunch of test files of using local/far pointers and containers (and few other system components). You can also treat those tests as examples of using AIFM. `aifm/test.sh` is a script that runs all tests automatically. It includes the commands of running AIFM end-to-end.
```
./test.sh
```

## Reproduce Experiment Results
We provide code and scripts in `aifm/exp` folder for reproducing our experiments. For more details, see `aifm/exp/README.md`.

## Repo Structure

```
Github Repo Root
 |---- build_all.sh  # A push-button build script for building both Shenango and AIFM.
 |---- shenango      # A modified version of Shenango runtime for AIFM. DO NOT USE OTHER VERSIONS.
 |---- aifm          # AIFM code base.
        |---- bin       # Test binaries and a TCP server ran at the remote memory node.
        |---- configs   # Configuration files for running AIFM.
        |---- inc       # AIFM headers.
        |---- src       # AIFM cpp files.
        |---- test      # Test files of using far-memory pointers and containers.
        |---- snappy    # An AIFM-enhanced snappy.
        |---- DataFrame # C++ DataFrame library (which includes both the original version and the AIFM version).
        |---- exp       # Code and scripts for reproducing our experiments.
        |---- Makefile
        |---- build.sh  # The script for building AIFM.
        |---- test.sh   # The script for testing AIFM.
        |---- shared.sh # A collection of helper functions for other scripts.
```

## Contact
Contact zainruan [at] csail [dot] mit [dot] edu for assistance.

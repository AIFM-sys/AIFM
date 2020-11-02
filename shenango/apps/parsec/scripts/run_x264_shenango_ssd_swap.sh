#!/bin/bash

sudo cp cgconfig.conf /etc/cgconfig.conf
sudo /usr/sbin/cgconfigparser -l /etc/cgconfig.conf
sudo /usr/sbin/cgrulesengd -vvv

sudo pkill -9 iokerneld
sudo rmmod uio_pci_generic
sudo rmmod nvme
sudo modprobe nvme
sleep 1
sudo mkswap /dev/nvme0n1
sudo swapon /dev/nvme0n1

export SHENANGODIR=/home/zain/far_mem/far_mem_shenango
sudo numactl --membind=0 --cpunodebind=0 $SHENANGODIR/iokerneld &
sudo rm -rf /home/zain/far_mem/far_mem_shenango/apps/parsec/pkgs/apps/x264/inst
sudo rm -rf /home/zain/far_mem/far_mem_shenango/apps/parsec/pkgs/apps/x264/obj
sudo rm -rf /home/zain/far_mem/far_mem_shenango/apps/parsec/pkgs/apps/x264/run
cd ../bin
./parsecmgmt -a build -p x264 -c gcc-shenango

mkdir -p ../pkgs/apps/x264/run
sudo cgexec -g memory:tester numactl --membind=0 --cpunodebind=0 stdbuf -o0 \
../pkgs/apps/x264/inst/amd64-linux.gcc-shenango/bin/x264 ../scripts/shenango_swap.cfg \
--quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb \
--mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads 22 \
-o ../pkgs/apps/x264/run/netflix.264 \
../pkgs/apps/x264/inputs/Netflix_Dancers_4096x2160_60fps_8bit_420.y4m | tee .stdout &

( tail -f -n0 .stdout & ) | grep -q "encoded"
sudo pkill -9 iokerneld
sudo swapoff /dev/nvme0n1
sudo $SHENANGODIR/spdk/scripts/setup.sh
sudo rm -f .stdout
sudo pkill -9 x264
sudo pkill -9 parsecmgmt

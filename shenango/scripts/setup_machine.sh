#!/bin/bash
# run with sudo

# needed for the iokernel's shared memory
sysctl -w kernel.shm_rmid_forced=1
sysctl -w kernel.shmmax=18446744073692774399
sysctl -w vm.hugetlb_shm_group=27
sysctl -w vm.max_map_count=16777216
sysctl -w net.core.somaxconn=3072

# set up the ksched module
rmmod ksched
rm /dev/ksched
insmod $(dirname $0)/../ksched/build/ksched.ko
mknod /dev/ksched c 280 0
chmod uga+rwx /dev/ksched
rm /dev/pcicfg
mknod /dev/pcicfg c 281 0
chmod uga+rwx /dev/pcicfg

# reserve huge pages
echo 8192 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

# revoke any CAT settings
modprobe msr
if command -v pqos &> /dev/null
then
  pqos -R l3cdp-any
fi

# enable RDPMC instruction from userspace
echo 2 > /sys/devices/cpu/rdpmc

# THP is only used with madvise
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled

# Enable turbo boost
echo 0 > /sys/devices/system/cpu/intel_pstate/no_turbo

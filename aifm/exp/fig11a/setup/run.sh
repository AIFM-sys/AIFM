#!/bin/bash

DISK=/dev/sda4
SHELL_PWD=`pwd`

# Format disk.
sudo umount $DISK
echo Y | sudo mkfs.ext4 $DISK
sudo mount $DISK /mnt
sudo chmod a+rw /mnt
cd /mnt

# Download ucompressed input.
wget http://cs.fit.edu/~mmahoney/compression/enwik9.zip
unzip enwik9.zip
mv enwik9 enwik9.uncompressed

# Create compressed input.
cd $SHELL_PWD
make clean
make
./main

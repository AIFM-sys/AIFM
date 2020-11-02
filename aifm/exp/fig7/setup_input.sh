#!/bin/bash

# The total input file size is about 16 GB.
# The max working set size in memory is about 31 GiB.
links=(https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2016-01.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2016-02.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2016-03.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2016-04.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2016-05.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2016-06.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2015-01.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2015-02.csv \
       https://s3.amazonaws.com/nyc-tlc/trip+data/yellow_tripdata_2015-03.csv)

head=yellow_tripdata_2016-01.csv

sudo umount /dev/sda4
sudo mkfs.ext4 -F /dev/sda4
sudo mount /dev/sda4 /mnt
sudo chmod a+rw /mnt
cd /mnt

for link in "${links[@]}"
do
    wget "$link"
done

cat $head > all.csv

for file in `ls *.csv`
do
    if [ "$file" = "all.csv" ]; then
	continue
    fi
    if [ "$file" = "$head" ]; then
	continue
    fi
    awk '{if (NR > 1) print $0}' $file >> all.csv
done

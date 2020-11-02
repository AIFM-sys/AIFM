#!/bin/bash

source ../../../shared.sh

zipf_s_arr=(0 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1 1.05 1.1 1.15 1.2 1.25 1.3 1.35)

sudo pkill -9 main
for zip_s in ${zipf_s_arr[@]}
do
    sed "s/constexpr static double kZipfParamS.*/constexpr static double kZipfParamS = $zip_s;/g" main.cpp -i
    make clean
    make -j
    rerun_local_iokerneld
    run_program ./main 1>log.$zip_s 2>&1
done
kill_local_iokerneld

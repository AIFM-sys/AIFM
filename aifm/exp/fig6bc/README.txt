The goal of this experiment is to show that i) the AIFM (NT) performance increases rapidly (and finally reaches 100%) when the local memory ratio increases; ii) the array and hashtable miss rates drop when the local memory ratio increases. And AIFM (NT) prioritizes hashtable over array in local memory space.

The local-only throughput is taken from fig6a which is about 0.42 MOPS. The script of this folder generates the data points of the "AIFM (NT)" line of fig 6b and the entire fig 6c.

First, execute run.sh. It generates a bunch of log.X files where X is the local memory size (in GB) used in the execution. The local memory ratio is calculated as X / 1024.0 / 26. Each log file contains three lines: the absolute throughput (in MOPS), the hashtable miss rate, and the array miss rate. Note that you have to normalize the throughput by the ideal local-only case to get the fig 6b line.


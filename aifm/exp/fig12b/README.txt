The goal of this experiment is to show AIFM adds very low runtime overhead on dereferencing cold objects. The latency it achieves is very close to the time spent on TCP transfer.

"64" corresponds to the second row of fig 10b.
"4096" corresponds to the third row of fig 10b.

Take the "64" folder for instance, it has 4 sub-folders. First, execute "run.sh" scripts of "read_total" and "write_total" to get the total cycles numbers in the paper. Then execute the scripts of "read_tcp" and "write_tcp" to get cycles spent on TCP transfer in the paper. The overheads numbers are calculated as total_cycles - TCP_cycles.

The goal of this experiment is to show AIFM is able to achieve an optimal performance (defined as the performance of running everything in local memory) when the application has a moderate compute intensity.

The "run.sh" scripts sweep the nanoseconds of compute per far memory access, and print the end-to-end execution time in microseconds. The results are generated as a bunch of {log.X} files. For example, log.1000 contains the application execution time when there are 1000 nanoseconds (i.e., 1 microsecond) of compute per far memory access.


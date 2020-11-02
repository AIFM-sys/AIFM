The goal of this experiment is to show AIFM achieves a near-optimal performance on Snappy compression and decompression.

First, execute "setup/run.sh" to download and generate input for this experiment.

Second, execute "run.sh" scripts. For "linux_mem/run.sh", it generates a single number representing the execution time (in microseconds) when the entire working set fits in local memory. For "aifm/run.sh", it sweeps the local memory size and prints the corresponding execution time. The results are a bunch of {log.X} files. For example, log.256 means the execution time when using 256 MB local memory.

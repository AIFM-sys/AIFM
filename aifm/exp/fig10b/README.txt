The goal of this experiment is to show the scalability of AIFM's far-memory hashtable. Having more threads benefits from AIFM's fast user-level context switching capability for hiding TCP latency, therefore, achieves a higher throughput. The performance plateaus after having 200 threads since NIC is saturated.

The "run.sh" script sweeps the number of threads and prints the corresponding throughput. The results are a bunch of {log.X} files. For example, log.10 prints the throughput when using 10 threads.

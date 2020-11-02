The goal of this experiment is to show AIFM achieves a near-optimal hashtable performance when the request distribution is skewed (i.e., has a high zipf skew parameter).

The hashtable is defined at "aifm/inc/concurrent_hopscotch.hpp". It has a C-style GenericConcurrentHopscotch to support dynamic-size key-value pairs. It also has a C++-style wrapper ConcurrentHopscotch<K, V> for the case when key and value are statically typed. This experiment involves dynamic-size requests, so we use GenericConcurrentHopscotch here.

The "run.sh" scripts in subfolders sweep the zipf skew parameter and prints the application throughput (in MOPS). The results are a bunch of {log.X} files. For example, log.0 contains the throughput when zipf parameter is 0.

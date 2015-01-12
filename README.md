# CAF Benchmark Suite

The CAF benchmark suite consits of a set of microbenchmarks implemented for various platforms and shell scripts to generate the results. Please note that the shell scripts are only tested under Linux.

## Run Benchmark Suite

You may run all benchmarks as root using `script/caf_run_benchmarks`.

## Scripts and Files

Implementations of all benchmark programs can be found under `src/$PLATOFRM`. Utility scripts required to run the benchmark suite can be found in `scripts`. Note that some scripts are generated from `src/scripts` and are only available after the CMake setup.

* `scripts/activate_cores` activates a given number of CPU cores
* `script/run` starts a single benchmark program
* `script/caf_run_benchmarks` runs the benchmark suite

The benchmark suite also contains two C++ tool applications.

* `tools/caf_run_bench.cpp` measure runtime and memory consumption for a single benchmark program
* `tools/to_dat.cpp` converts the raw output from `caf_run_bench` into CSV files that can be plottet

## Add a benchmark

Add implementations for a new platform to `src/$PLATOFRM`, add the building steps to CMake, and adjust `run` by adding a section under `case "$impl" ...` for your benchmarks.

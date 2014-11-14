# CAF Benchmarks
## Build your benchmarks
You may run all benchmarks with `sudo ./script/caf_run_benchmarks.sh`.

## Scripts
Under `src` you can find the benchmark programs. All script files and a few source files we use to run the benchmarks are at located under `script`.

* `script/caf_run_benchmarks.sh` 
* `script/run` starts `caf_run_bench` with arguments for the benchmark implementations.
* `script/caf_run_bench.cpp` writes benchmark results to `/local_data/`.

* `script/activate_cores.sh`  allows you to turn cores on and off.

## Add a benchmark
Add your implementation to `src` and adjust `run`. Therefore add a section under `case "$impl" in` for your benchmark.

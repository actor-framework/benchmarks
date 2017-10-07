#!/bin/bash

./caf_run_benchmarks.py --bench-user=XXX --out-dir=XXX --bin-path="/users/localadmin/woelke/actor-framework/build/bin" --min-cores=4 --max-cores=64 --repetitions=2 --metric-script="./metrics/metric_template.py" --label=label --label-suffix="lsuffix" --bench="numa_matrix_search_continues" --custom-args="--searches=1"




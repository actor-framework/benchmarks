#!/usr/bin/python3

import os
import sys
import argparse
# import importlib.util
import importlib.machinery
import numpy as np
import scipy as sp
import scipy.stats
import subprocess
import copy

def execute_cmd(cmd):
    out = subprocess.getoutput(cmd)
    return [line for line in out.split(sep="\n")]

def parse_all_args():
    parser = argparse.ArgumentParser(description="Run a benchmark") 
    parser.add_argument("--bench-user", dest="bench_user", nargs=1, required=True)
    parser.add_argument("--out-dir", dest="out_dir", nargs=1, required=True)
    parser.add_argument("--bin-path", dest="bin_path", nargs=1, required=True)
    parser.add_argument("--min-cores", type=int, dest="min_cores", nargs=1, required=True)
    parser.add_argument("--max-cores", type=int, dest="max_cores", nargs=1, required=True)
    parser.add_argument("--pass_core_count_as_arg", dest="pass_core_count_as_arg", action="store_true")
    parser.add_argument("--repetitions", type=int, dest="repetitions", nargs=1, required=True)
    parser.add_argument("--metric-script", dest="metric_script", nargs=1, required=True)
    parser.add_argument("--label", dest="label", nargs=1)
    parser.add_argument("--label-suffix", dest="label_suffix", nargs="?")
    parser.add_argument("--bench", dest="bench", nargs=1, required=True)
    parser.add_argument("--custom-args", dest="args", nargs="*")
    return parser.parse_args()

def run_bench(bench_module, args):
    print(" Bench: {}".format(args.bench[0]))
    raw_data= []
    for i in range(0, args.repetitions[0]):
        print("{} ".format(i), end="", flush=True)
        raw_data.append(bench_module.run_as_benchmark(args))
    # data = [{local-accesses: 7, remote-accesses: 9}, ...] to {la: [12,3,4], ...}
    data = dict()
    for d in raw_data:
        for label, value in d.items():
            if label in data:
                data[label].append(value)
            else:
                data[label] = [value]
    print("\033[2K\r", end="")
    return data # {label1: [12,3,4], label2: ...}
        
def activate_cores(num):
    execute_cmd("./activate_cores " + str(num))

def import_metric_script(args):
    ms = args.metric_script[0]
    ms_file_name = os.path.basename(ms)
    # spec = importlib.util.spec_from_file_location(ms_file_name, ms)
    # mod = importlib.util.module_from_spec(spec)
    # spec.loader.exec_module(mod)
    # return mod
    mod = importlib.machinery.SourceFileLoader(ms_file_name, ms).load_module()
    return mod

def mean_confidence_interval(data, confidence=0.95):
    # something is wrong here!!!!!1
    a = 1.0*np.array(data)
    n = len(a)
    m, se = np.mean(a), scipy.stats.sem(a)
    h = se * sp.stats.t.ppf((1+confidence)/2., n-1)
    return h 

def raw_data_to_csv_strings(raw_data):
    # result:
    # cores            , caf-ms-stealing  , caf-ms-stealing_yerr , ...
    # 4                , 4482.8           , 130.522
    # 
    # raw_data
    # {num_of_cores: { label_a: [values], ... } , ...   }
    csv_strings = []
    #header 
    do_header = True
    yerr_str = "_yerr"
    # nice formating
    field_width = 0
    for label_data in raw_data.values():
        for label in label_data.keys():
            if field_width < len(label):
                field_width = len(label) + len(yerr_str)
    # content
    def pts(txt):
        return ("{0:<" + str(field_width) + "}").format(str(txt))
    for num_of_cores, label_data in sorted(raw_data.items()):
        tmp_str = ""
        if do_header:
            tmp_str += pts("cores") + ", "
            for label in sorted(label_data.keys()):
                tmp_str += pts(label) + ", " + pts(label + yerr_str) + ", "
            do_header = False
            csv_strings.append(tmp_str[:-2])
            tmp_str = ""
        tmp_str += pts(num_of_cores) + ", "
        for label, values in sorted(label_data.items()):
            mean = sum(values) / len(values)
            conf_interval = mean_confidence_interval(values)
            tmp_str += pts(mean) + ", " + pts(conf_interval) + ", "
        csv_strings.append(tmp_str[:-2])
    return  csv_strings

def write_data_to_file(filename, data):
    f = open(filename, "w")
    for line in data:
        f.write(str(line) + "\n")


def write_raw_data_to_file(args, raw_data):
    # raw_data:
    # {num_of_cores: { label_a: [values], ... } , ...   }
  
    # file format:
    # "{X-VALUE}_{X-LABEL}_{MEMORY_OR_RUNTIME}_{LABEL-labelsuffix}_{BENCHMARK}\\.txt";
      # replace_all(fstr, "{X-VALUE}", "([0-9]+)");
      # replace_all(fstr, "{X-LABEL}", "([a-zA-Z_\\-]+)");
      # replace_all(fstr, "{MEMORY_OR_RUNTIME}", "(runtime|memory_[0-9]+)");
      # replace_all(fstr, "{LABEL}", "([a-zA-Z0-9\\-]+)");
      # replace_all(fstr, "{BENCHMARK}", "([a-zA-Z0-9_\\-]+)");
    
    # examples:
    # 60_cores_runtime_caf-ms-CLS-original_numa_matrix_search_continues.txt
    # 60_cores_runtime_caf-ms-CLS-thread-pinning_numa_matrix_search_continues.txt
    for num_of_cores, label_data in raw_data.items():
        for label, values in label_data.items():
            filename = "{cores}_cores_runtime_{label_suf}_{bench}.txt".format(cores=num_of_cores, label_suf=args.label[0] + "-" + args.label_suffix + "-" + label, bench = args.bench[0])
            write_data_to_file(args.out_dir[0] + "/" + filename, values)

def run():
    if os.geteuid() != 0:
        print("you need to be root")
        exit(0)
    raw_data = dict() # {num_of_cores: { label_a: [values], ... } , ...   }
    args = parse_all_args()
    bench_module = import_metric_script(args)
    print("-- Label: {} {}".format(args.label[0], args.label_suffix))
    for num_of_cores in range(args.min_cores[0], args.max_cores[0] + 1, args.min_cores[0]):
        tmp_args = copy.deepcopy(args)
        if tmp_args.pass_core_count_as_arg:
            tmp_args.args[0] += " --caf#scheduler.max-threads=" + str(num_of_cores)
        else:
            activate_cores(num_of_cores)
        print("Cores: {}".format(num_of_cores))
        raw_data[num_of_cores] = run_bench(bench_module, tmp_args)
    write_raw_data_to_file(args, raw_data)

if __name__ == "__main__":
    run()

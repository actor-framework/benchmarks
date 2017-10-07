#!/usr/bin/python3

import io
import sys
import subprocess

def print_to_string(*args, **kwargs):
    output = io.StringIO()
    print(*args, file=output, **kwargs)
    contents = output.getvalue()
    output.close()
    return contents

def execute_cmd(cmd, default_execution_path="./"):
    change_dir = "cd " + default_execution_path + "; "
    out = subprocess.getoutput(change_dir + cmd)
    return [line for line in out.split(sep="\n")]

class perfctr_request(object):
    pass

def likwid_perfctr_cmd(perf_req):
    def hardware_counter_to_string():
        tmp_str = ""
        for counter_id, what_to_count in zip(range(0, len(perf_req.what_to_counts)),perf_req.what_to_counts):
            tmp_str += what_to_count[0] + ":" + what_to_count[1] + str(counter_id) + ","
        tmp_str = tmp_str[:-1] + " "
        return tmp_str
    # likwid_program = "likwid-perfctr -f -O "
    likwid_program = "/usr/local/bin/likwid-perfctr -f -O "
    perf_req.cmd = []
    for where_to_count in perf_req.where_to_counts:
        perf_req.cmd.append(likwid_program + "-c " + where_to_count + " -g " + hardware_counter_to_string() + " ")
    return perf_req

def load_raw_data(cmd, perf_req, default_execution_path="./"):
    perf_req = likwid_perfctr_cmd(perf_req)
    raw_data = []
    for perf_cmd in perf_req.cmd:
        raw_data.extend(execute_cmd(perf_cmd + cmd, default_execution_path)) 
    return raw_data

# parse the raw data
def get_accesses_per_core(raw_data, perf_req):
    def get_search_lines():
        result = dict()  #{LOCAL ACCESSES: UNC_CPU_REQUEST_TO_MEMORY_LOCAL_LOCAL_CPU_MEM,UPMC0, ..}
        for i in range(0, len(perf_req.names_of_what_to_counts)):
            result[perf_req.names_of_what_to_counts[i]] = perf_req.what_to_counts[i][0] + "," + perf_req.what_to_counts[i][1] + str(i)
        return result
    def fill_counter(data, counters):
        if len(data) < 2:
            return
        for line_idx in range(0, len(data), 2):
            for column_idx in range(2, len(data[line_idx])):
                header_line = data[line_idx]
                value_line = data[line_idx+1]
                dict_value = int(header_line[column_idx][5:])
                counters[dict_value] = int(value_line[column_idx])
    counters = dict()
    data = dict()
    for name in perf_req.names_of_what_to_counts:
        counters[name] = dict()
        data[name] = []
    for line in raw_data:
        if "Event,Counter,Core" in line:
            for vals in data.values():
                vals.append(line.split(","))
        for name_of_what_to_count, search_for in get_search_lines().items():
            if search_for in line:
                data[name_of_what_to_count].append(line.split(","))
    for name in perf_req.names_of_what_to_counts:
        fill_counter(data[name], counters[name])
    return counters

def plot_data(data, perf_req):
    def to_string_in_million(v):
        return str(int((v/1e6)+0.5))
    def sum_events(d):
        return sum([v for v in d.values()])
    def data_to_string(d):
        key_line = print_to_string("%13s" %("Core: "), end='')
        value_line = print_to_string("%13s" %("Mega events: "), end='')
        key_line += print_to_string("%7s" %("SUM"), end='')
        value_line += print_to_string("%7s" %(to_string_in_million(sum_events(d))), end='')
        for k, v in sorted(d.items()):
            key_line += print_to_string("%6s" %(str(k)), end='')
            value_line += print_to_string("%6s" %(to_string_in_million(v)), end='')
        return key_line + "\n" + value_line
    for name_of_what_to_count in data.keys():
        print(name_of_what_to_count + "\n" + data_to_string(data[name_of_what_to_count]))
        print("")

def load_dummy_raw_data():
    f = open("test_data/mem_access_plot_a.txt", "r")
    result = [line.strip() for line in f]
    f = open("test_data/mem_access_plot_b.txt", "r")
    result.extend([line.strip() for line in f])

    # f = open("test_data/cache_access_plot_a.txt", "r")
    # result = [line.strip() for line in f]
    return result 

def run(args):
    cmd = " ".join(args)
    # raw_data = load_dummy_raw_data()
    # a = "likwid-perfctr -f -O -c 0,16,32,48 -g UNC_CPU_REQUEST_TO_MEMORY_LOCAL_LOCAL_CPU_MEM:UPMC0,UNC_CPU_REQUEST_TO_MEMORY_LOCAL_REMOTE_CPU_MEM:UPMC1,DATA_CACHE_ACCESSES:PMC0,DATA_CACHE_MISSES_ALL:PMC1 "
    # b = "likwid-perfctr -f -O -c 8,24,40,56 -g UNC_CPU_REQUEST_TO_MEMORY_LOCAL_LOCAL_CPU_MEM:UPMC0,UNC_CPU_REQUEST_TO_MEMORY_LOCAL_REMOTE_CPU_MEM:UPMC1,DATA_CACHE_ACCESSES:PMC0,DATA_CACHE_MISSES_ALL:PMC1 "

    perf_req = perfctr_request()
    perf_req.where_to_counts = ["0,16,32,48","8,24,40,56"]
    perf_req.names_of_what_to_counts = ["LOCAL-ACCESSES", "REMOTE-ACCESSES"]
    perf_req.what_to_counts = [("UNC_CPU_REQUEST_TO_MEMORY_LOCAL_LOCAL_CPU_MEM","UPMC"), ("UNC_CPU_REQUEST_TO_MEMORY_LOCAL_REMOTE_CPU_MEM", "UPMC")]

    # perf_req = perfctr_request()
    # perf_req.where_to_counts = ["0-3"]
    # perf_req.names_of_what_to_counts = ["CACHE-ACCESSES", "CACHE-MISSES"]
    # perf_req.what_to_counts = [("DATA_CACHE_ACCESSES","PMC"), ("DATA_CACHE_MISSES_ALL", "PMC")]

    # raw_data = load_raw_data(cmd, perf_req)
    raw_data = load_dummy_raw_data()
    for line in raw_data:
        print(line)
    data = get_accesses_per_core(raw_data, perf_req)
    print(data)
    plot_data(data, perf_req)

def run_as_benchmark(args):
    perf_req = perfctr_request()
    ## measure memory-accesses
    perf_req.where_to_counts = ["0,16,32,48","8,24,40,56"]
    perf_req.names_of_what_to_counts = ["LOCAL-ACCESSES", "REMOTE-ACCESSES"]
    perf_req.what_to_counts = [("UNC_CPU_REQUEST_TO_MEMORY_LOCAL_LOCAL_CPU_MEM","UPMC"), ("UNC_CPU_REQUEST_TO_MEMORY_LOCAL_REMOTE_CPU_MEM", "UPMC")]
    ## measure cache access
    # perf_req.where_to_counts = ["0-63"]
    # perf_req.names_of_what_to_counts = ["CACHE-ACCESSES", "CACHE-MISSES"]
    # perf_req.what_to_counts = [("DATA_CACHE_ACCESSES","PMC"), ("DATA_CACHE_MISSES_ALL", "PMC")]

    cmd = "./" + args.bench[0] + " " + args.args[0]
    raw_data = load_raw_data(cmd, perf_req, args.bin_path[0])
    # raw_data = load_dummy_raw_data()
    data = get_accesses_per_core(raw_data, perf_req)
    result = dict()
    for name_of_what_to_count in perf_req.names_of_what_to_counts:
        result[name_of_what_to_count] = sum(data[name_of_what_to_count].values())
    return result # {local-accesses: 7, remote-accesses: 9}


if __name__ == "__main__":
    run(sys.argv[1:])


#!/usr/bin/python3

import io
import sys
import subprocess

def execute_cmd(cmd, default_execution_path="./"):
    change_dir = "cd " + default_execution_path + "; "
    out = subprocess.getoutput(change_dir + cmd)
    return [line for line in out.split(sep="\n")]

def parse(raw_data):
    data = [int(v.strip()) for v in raw_data[-1].split(",")]
    return {"sched-events": data[0], "steal-attempts": data[1], "success-steals": data[2]}

def run(args):
    cmd = " ".join(args)
    raw_data = execute_cmd(cmd)
    print(raw_data)
    data = parse(raw_data)
    print(data)

def run_as_benchmark(args):
    cmd = "./" + args.bench[0] + " " + args.args[0]
    raw_data = execute_cmd(cmd, args.bin_path[0])
    data = parse(raw_data)
    return data # e.g.: {'steal-attempts': 1115, 'success-steals': 16, 'sched-events': 13727}

if __name__ == "__main__":
    run(sys.argv[1:])


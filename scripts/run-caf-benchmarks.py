#!/usr/bin/env python3

import argparse
import os
import statistics
import subprocess
import sys
import time

DEFAULT_BIN_PATH = "./build/bin"
REPETITIONS = 10

BENCHMARKS = [
    ("mixed_case", ["100", "100", "1000", "4"]),
    ("actor_creation", ["20"]),
    ("mailbox_performance", ["100", "1000000"]),
]


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run CAF benchmark binaries with the default benchmark parameters."
    )
    parser.add_argument(
        "--bin-path",
        default=DEFAULT_BIN_PATH,
        help=f"folder of benchmark executables (default: {DEFAULT_BIN_PATH})",
    )
    return parser.parse_args()


def run_once(bin_path, args):
    start = time.perf_counter()
    result = subprocess.run(
        [bin_path, *args],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    elapsed = time.perf_counter() - start
    return elapsed, result.returncode


def run_benchmark(name, args, bin_dir):
    bin_path = os.path.join(bin_dir, name)
    if not os.path.isfile(bin_path) or not os.access(bin_path, os.X_OK):
        print(f"benchmark not found or not executable: {bin_path}", file=sys.stderr)
        sys.exit(1)

    arg_str = " ".join(args)

    runtimes = []
    for _ in range(REPETITIONS):
        elapsed, status = run_once(bin_path, args)
        if status != 0:
            print(f"{name} failed (exit status {status})", file=sys.stderr)
            sys.exit(status)
        runtimes.append(elapsed)

    return name, arg_str, runtimes


def print_summary(name, arg_str, runtimes):
    print(
        f"{name} {arg_str}: "
        f"min={min(runtimes):.3f}s  "
        f"max={max(runtimes):.3f}s  "
        f"avg={statistics.mean(runtimes):.3f}s"
    )


def main():
    args = parse_args()
    bin_dir = args.bin_path

    if not os.path.isdir(bin_dir):
        print(f"bin path not found: {bin_dir}", file=sys.stderr)
        sys.exit(1)

    results = []
    for name, bench_args in BENCHMARKS:
        results.append(run_benchmark(name, bench_args, bin_dir))

    for name, arg_str, runtimes in results:
        print_summary(name, arg_str, runtimes)


if __name__ == "__main__":
    main()

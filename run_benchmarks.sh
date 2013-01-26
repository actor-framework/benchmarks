#!/bin/bash

if [[ $(uname) == "Darwin" ]]; then
  NumCores=$(/usr/sbin/system_profiler SPHardwareDataType | awk 'tolower($0) ~ /total number of cores/ {print $5};')
else
  NumCores=$(grep "processor" /proc/cpuinfo | wc -l)
fi

if [ $NumCores -lt 10 ]; then
    NumCores="0$NumCores"
fi

# arguments for all benchmarks
mixed_case="20 50 10000 5"
actor_creation="20"
mailbox_performance="20 1000000"

for lang in "cppa" "scala" "erlang" "go" ; do
    for bench in "actor_creation" "mailbox_performance" "mixed_case" ; do
        Args=${!bench}
        echo "$lang: $Impl $bench ..." >&2
        FileName="$NumCores cores, $lang $bench.txt"
        exec 1>>"$FileName"
#        for i in {1..5} ; do
            ./run $lang $bench $Args
#        done
    done
done


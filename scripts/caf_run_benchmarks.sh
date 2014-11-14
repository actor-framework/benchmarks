#!/bin/bash

CAF_HOME=/home/neverlord/actor-framework
CAF_BINS=$CAF_HOME/build/bin
OUT_DIR=/local_data

# arguments for all benchmarks
mixed_case="100 100 1000 4"
actor_creation="20"
mailbox_performance="100 1000000"
mandelbrot="16000"

#for lang in caf charm erlang scala foundry ; do
for lang in caf ; do
  for NumCores in $(seq 4 4 64); do
    /home/neverlord/activate_cores.sh $NumCores
    fname=$(printf "%.2i_cores" $NumCores)
    for bench in mixed_case actor_creation mailbox_performance ; do
      echo "$lang: $bench @ $NumCores cores"
      args=${!bench}
      runtimes="$OUT_DIR/${fname}_runtime_${lang}_${bench}.txt"
      for i in {1..10} ; do
        memfile="$OUT_DIR/${fname}_memory_${i}_${lang}_${bench}.txt"
        if [ -f "$memfile" ] ; then
          echo "SKIP $lang $bench $i (mem file already exists)"
        else
          $CAF_HOME/benchmarks/run neverlord $CAF_BINS $runtimes $memfile $lang $bench $args
        fi
      done
    done
  done
done


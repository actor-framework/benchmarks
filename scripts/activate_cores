#!/bin/bash

if [ "$(whoami)" != "root" ]; then
  echo "sorry, need to be root"
  exit
fi

if [ $# != 1 ]; then
  echo "usage: $0 NumberOfCores"
  exit
fi

NewNumCores=$1

if (( $NewNumCores < 1 )); then
  echo "cannot activate less than one core"
  exit
fi

MaxCores=$(lscpu | grep -E "^CPU\(s\)" | grep -oE "[0-9]+")
NumCPUs=$(lscpu | grep -E "^Socket\(s\)" | grep -oE "[0-9]+")
CoresPerCPU=$(echo "$MaxCores / $NumCPUs" | bc)
MaxCPUId=$(echo "$NumCPUs - 1" | bc)

if (( $MaxCores < $NewNumCores )); then
  echo "this machine only has $MaxCores cores"
  exit
fi

echo "max cores: $MaxCores"
echo "num CPUs: $NumCPUs"
echo "cores / CPU: $CoresPerCPU"

NumCores=$(grep "processor" /proc/cpuinfo | wc -l)
MaxCoreId=$(echo "$NumCores - 1" | bc)

NewMaxCoreId=$(echo "$1 - 1" | bc)

echo "currently running on $NumCores cores, switch to $NewNumCores cores"

if [ $NumCores = $NewNumCores ]; then
  echo "do nothing (already running on $NewNumCores cores)"
else
  ActiveCoresPerCPU=$(echo "$NewNumCores / $NumCPUs" | bc)
  ResCheck=$(echo "$ActiveCoresPerCPU * $NumCPUs" | bc)
  if (( $ResCheck != $NewNumCores )); then
    echo "cannot split $NewNumCores cores evenly across $NumCPUs processors"
    exit
  fi
  echo "enable $ActiveCoresPerCPU cores per CPU"
  for i in $(seq 0 $MaxCPUId); do
    id0=$(echo "$i * $CoresPerCPU" | bc)
    idX=$(echo "$id0 + $ActiveCoresPerCPU - 1" | bc)
    idN=$(echo "$id0 + $CoresPerCPU - 1" | bc)
    for j in $(seq $id0 $idX); do
      if [ $j != "0" ]; then
        # core0 cannot be disabled
        fname="/sys/devices/system/cpu/cpu$j/online"
        IsActive=$(cat "$fname")
        if [ $IsActive != "1" ]; then
      	  echo "activate core $j"
      	  echo 1 > "$fname"
        else
          echo "core $j already active"
        fi
      else
        echo "skip core 0 (cannot be deactivated)"
      fi
    done
    if (( $idX < $idN )); then
      idM=$(echo "$idX + 1" | bc)
      for j in $(seq $idM $idN); do
        fname="/sys/devices/system/cpu/cpu$j/online"
        IsActive=$(cat "$fname")
        if [[ $IsActive = "1" ]]; then
          echo "deactivate core $j"
          echo 0 > "$fname"
        else
          echo "core $j already inactive"
        fi
      done
    fi
  done
fi


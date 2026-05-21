#!/bin/bash

NP="${NP:-3}"
HOSTFILE="${HOSTFILE:-/mnt/cluster/hostfile}"
BIN="${BIN:-/mnt/cluster/mul}"
OUT="${OUT:-/mnt/cluster/times2.txt}"

> "$OUT"

for j in {1..10}
do
    for i in 1000 1259 1586 1996 2513 3163 3981 5010 6310
    do
        mpirun --hostfile "$HOSTFILE" --map-by node -np "$NP" "$BIN" "$i" >> "$OUT"
    done
done

#!/bin/bash

> times2.txt

for j in {1..10}
do
    for i in 1000 1259 1586 1996 2513 3163 3981 5010 6310 
    do
        ./mul $i >> times2.txt
    done
done

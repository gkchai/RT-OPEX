#!/bin/bash

exp=(O P)
samples=(1000 2000 3000)
sched=(F R)

for ((e=0; e<2; e=e+1)); do
    for ((s=0; s<3; s=s+1)); do
        for ((h=0; h<2; h=h+1)); do

            # TODO: run matlab scripts for transport
            sudo ./gd.o -n 4 -s "${samples[s]}" -d 10 -p 99 -S "${sched[h]}" -e "${exp[e]}"
        done
    done
done
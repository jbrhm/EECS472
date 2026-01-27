#!/usr/bin/env bash

echo "" > output

for file in /home/jabra/EECS472/p2_472-w26.jabra/output/*.vcd; do
    echo "${file}"
    ./a.out --vcd "${file}" --clock "testbench.verisimpleV.clock" >> output
done


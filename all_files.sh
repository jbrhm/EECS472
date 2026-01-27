#!/usr/bin/env bash

echo "" > output_file

for file in output/*.vcd; do
    echo "${file}"
    ./a.out --vcd "${file}" --clock "testbench.verisimpleV.clock" >> output_file
done

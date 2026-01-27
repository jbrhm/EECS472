#!/usr/bin/env bash

echo "" > output

for file in *.vcd; do
    echo "${file}"
    ./a.out --vcd "${file}" >> output
done


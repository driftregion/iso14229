#! /bin/bash

files=`find . -type f \( -name '*.c' -o -name '*.h' \) -not -path "./isotp-c/*"`

for file in $files ; do
    clang-format -i $file
done

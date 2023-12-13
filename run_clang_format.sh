#! /bin/bash

files=`find src -type f \( -name '*.c' -o -name '*.h' \) -not -path "src/tp/isotp-c/*"`

for file in $files ; do
    if [ -z "$CHECK_FORMAT" ] ; then
        clang-format -i $file
    else
        clang-format -Werror --dry-run $file
    fi
done

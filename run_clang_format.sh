#! /bin/bash

files=`find . -type f \( -name '*.c' -o -name '*.h' \) -not -path "./tp/isotp-c/*"`

for file in $files ; do
    if [ -z "$CHECK_FORMAT" ] ; then
        clang-format -i $file
    else
        clang-format -Werror --dry-run $file
    fi
done

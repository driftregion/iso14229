#!/bin/bash

CodeChecker analyze compile_commands.json \
    --ignore tools/CodeChecker/skipfile_bazel.txt \
    --ignore tools/CodeChecker/skipfile_thirdparty.txt \
    --file "iso14229.c" \
    -o reports 

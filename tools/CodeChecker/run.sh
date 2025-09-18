#!/bin/bash

# Unified CodeChecker analysis: clang-tidy, clangsa, and MISRA (cppcheck) all together
CodeChecker analyze compile_commands.json \
    --ignore tools/CodeChecker/skipfile_bazel.txt \
    --ignore tools/CodeChecker/skipfile_thirdparty.txt \
    --file "iso14229.c" \
    --analyzer-config cppcheck:addons=tools/cppcheck/misra.json \
    --analyzer-config cppcheck:platform=unix64 \
    --analyzer-config cppcheck:cc-verbatim-args-file=tools/CodeChecker/cppcheckargs.txt \
    -o reports 

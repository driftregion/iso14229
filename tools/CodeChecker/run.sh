#!/bin/bash

CodeChecker analyze compile_commands.json \
    --ignore tools/CodeChecker/skipfile_bazel.txt \
    -o reports

# MISRA checking via CodeChecker isn't working properly. Use tools/cppcheck/run.sh instead.
# CodeChecker analyze compile_commands.json \
#     --ignore tools/CodeChecker/skipfile_bazel.txt \
#     --ignore tools/CodeChecker/skipfile_thirdparty.txt \
#     --cppcheckargs tools/CodeChecker/cppcheckargs.txt \
#     --analyzer-config cppcheck:addons=tools/cppcheck/misra.json \
#     --analyzer-config cppcheck:platform=unix64 \
#     -o misra_reports 
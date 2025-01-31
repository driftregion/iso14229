#!/bin/bash

# --enable sensitive \
# --analyzers cppcheck \
CodeChecker analyze compile_commands.json \
    --ignore .CodeChecker/skipfile.txt \
    --analyzer-config cppcheck:addons=.cppcheck/misra.json \
    -o reports
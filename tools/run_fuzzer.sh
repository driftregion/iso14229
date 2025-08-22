#!/bin/bash

# Exit on error
set -e

# bazel build //test:make_corpus
# bazel-bin/test/make_corpus

# bazel build -s --config=aarch64_linux_clang //test:test_fuzz_server

# Directory for coverage output
FUZZ_DIR="fuzz"
mkdir -p "$FUZZ_DIR"

FULL_CORPUS_DIR=$FUZZ_DIR/"full_corpus"
mkdir -p "$FULL_CORPUS_DIR"

# # Run the fuzzer to generate raw profile data
LLVM_PROFILE_FILE="$FUZZ_DIR/profraw" \
./bazel-bin/test/test_fuzz_server \
$FULL_CORPUS_DIR \
-max_len=8192 \
-max_total_time=6000 \
-jobs=2 \
-artifact_prefix="$FUZZ_DIR/"

# -dict=$FUZZ_DIR/dictionary.txt \
# -max_total_time=6
# # -jobs=2 \
tools/update_fuzzer_dict.py

# Merge raw profile data
llvm-profdata-15 merge -sparse "$FUZZ_DIR/profraw" -o "$FUZZ_DIR/profdata"

# Generate coverage report
llvm-cov-15 show ./bazel-bin/test/test_fuzz_server \
    -instr-profile="$FUZZ_DIR/profdata" \
    -format=html > "$FUZZ_DIR/coverage.html"

# Print a summary
llvm-cov-15 report ./bazel-bin/test/test_fuzz_server \
    -instr-profile="$FUZZ_DIR/profdata"

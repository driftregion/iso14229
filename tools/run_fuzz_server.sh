#!/bin/bash

set -e

FULL_CORPUS_DIR=test/fuzz_outputs/corpus

bazel build --config=fuzz //test:fuzz_server 
du -sh $FULL_CORPUS_DIR

# See full LibFuzzer flag documentation at:
# https://llvm.org/docs/LibFuzzer.html
bazel-bin/test/fuzz_server_bin \
-artifact_prefix=test/fuzz_outputs \
-max_total_time=60 \
$FULL_CORPUS_DIR

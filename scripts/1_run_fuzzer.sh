#!/bin/bash

# Get workspace path
WORKSPACE=$(bazel info workspace)

bazel run -c opt --config=hermetic-fuzz //fuzz:fuzz_server_bin -- \
-max_total_time=600 \
-artifact_prefix=${WORKSPACE}/fuzz/outputs/ \
-jobs=$(nproc) \
${WORKSPACE}/fuzz/outputs/corpus

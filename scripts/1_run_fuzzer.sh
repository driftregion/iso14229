#!/bin/bash

bazel run -c opt --config=asan-libfuzzer //fuzz:fuzz_server_run -- \
--timeout_secs=10 \
--fuzzing_output_root=$(bazel info workspace)/fuzz/outputs

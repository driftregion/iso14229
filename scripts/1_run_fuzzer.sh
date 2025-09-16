#!/bin/bash

bazel run -c opt --config=hermetic-fuzz //fuzz:fuzz_server_run -- \
--timeout_secs=10 \
--fuzzing_output_root=$(bazel info workspace)/fuzz/outputs

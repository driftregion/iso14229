#!/bin/bash

bazel build //src:iso14229.c //src:iso14229.h 
BAZEL_BIN="$(bazel info bazel-bin)"
cp --no-preserve=mode $BAZEL_BIN/src/iso14229.c $BAZEL_BIN/src/iso14229.h -t .
cp --no-preserve=mode $BAZEL_BIN/src/iso14229.c $BAZEL_BIN/src/iso14229.h -t examples/arduino_server/main
cp --no-preserve=mode $BAZEL_BIN/src/iso14229.c $BAZEL_BIN/src/iso14229.h -t examples/esp32_server/main
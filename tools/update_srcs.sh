#!/bin/bash

bazel build //src:iso14229.c //src:iso14229.h 
BAZEL_BIN="$(bazel info bazel-bin)"
cp --no-preserve=mode $BAZEL_BIN/src/iso14229.c $BAZEL_BIN/src/iso14229.h -t .
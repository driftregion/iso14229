#!/bin/bash

set -euo pipefail


# Run coverage with hermetic toolchain
bazel coverage --config=coverage //test:fuzz_server

mkdir -p reports
OUTFILE=reports/coverage_fuzz.lcov

cp -f "$(bazel info output_path)/_coverage/_coverage_report.dat" $OUTFILE
sed -E -i 's#^SF:bazel-out/.*/src/iso14229.c#SF:src/iso14229.c#' $OUTFILE
sed -E -i 's#^SF:bazel-out/.*/src/iso14229.h#SF:src/iso14229.h#' $OUTFILE
sed -i '1i TN:fuzz' $OUTFILE
ls -l $OUTFILE

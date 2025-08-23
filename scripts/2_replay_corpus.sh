#!/bin/bash

set -euxo pipefail

bazel coverage -c opt --config=fuzz //fuzz:fuzz_server
cp -f "$(bazel info output_path)/_coverage/_coverage_report.dat" coverage.lcov
# sed -E -i 's#^SF:bazel-out/.*/iso14229.c#SF:iso14229.c#' coverage.lcov
# sed -E -i 's#^SF:bazel-out/.*/iso14229.h#SF:iso14229.h#' coverage.lcov
ls -l coverage.lcov

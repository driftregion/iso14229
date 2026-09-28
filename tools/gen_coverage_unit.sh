#!/bin/bash

set -euo pipefail

bazel coverage \
--config=coverage \
--combined_report=lcov \
--instrumentation_filter='^//(src:iso14229|test:.*)$' \
--instrument_test_targets \
--experimental_collect_code_coverage_for_generated_files \
--test_output=errors \
//test:test_server \
//test:test_client \
//test:test_tp_isotp_compliance_c \
//test:test_tp_isotp_compliance_sock \
//test:test_tp_isotp_compliance_mock

mkdir -p reports
OUTFILE=reports/coverage_unit.lcov

cp -f "$(bazel info output_path)/_coverage/_coverage_report.dat" $OUTFILE
sed -E -i 's#^SF:bazel-out/.*/src/iso14229.c#SF:src/iso14229.c#' $OUTFILE
sed -E -i 's#^SF:bazel-out/.*/src/iso14229.h#SF:src/iso14229.h#' $OUTFILE
sed -i '1i TN:unit' $OUTFILE
ls -l $OUTFILE

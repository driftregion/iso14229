#!/bin/bash

set -euo pipefail

FUZZ=reports/coverage_fuzz.lcov
UNIT=reports/coverage_unit.lcov
MERGED=reports/coverage_merged.lcov

genhtml \
--function-coverage \
--branch-coverage \
--show-details \
--legend  \
--output-directory reports/coverage_unit \
--substitute 's|src/*||' \
$UNIT

genhtml \
--function-coverage \
--branch-coverage \
--show-details \
--legend  \
--output-directory reports/coverage_fuzz \
--substitute 's|src/*||' \
$FUZZ


lcov --branch-coverage \
--rc derive_function_end_line=0 \
-a $UNIT \
-a $FUZZ \
-o $MERGED

genhtml \
--parallel=4 \
--function-coverage \
--branch-coverage \
--show-details \
--legend  \
--output-directory reports/coverage \
--substitute 's|src/*||' \
$MERGED

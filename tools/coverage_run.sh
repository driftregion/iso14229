#!/bin/bash

bazel coverage \
--combined_report=lcov \
--instrumentation_filter='^//(:iso14229|test:.*)$' \
--instrument_test_targets \
--experimental_collect_code_coverage_for_generated_files \
--test_output=errors \
//test:all

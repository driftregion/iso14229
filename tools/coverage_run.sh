#!/bin/bash

bazel coverage \
--combined_report=lcov \
--instrumentation_filter='^//(:iso14229|test:.*)$' \
--instrument_test_targets \
--experimental_collect_code_coverage_for_generated_files \
--test_output=errors \
//test:test_server_tp_mock \
//test:test_client_tp_mock \
//test:test_tp_isotp_compliance_c \
//test:test_tp_isotp_compliance_sock \
//test:test_tp_isotp_compliance_mock

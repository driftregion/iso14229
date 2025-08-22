#!/bin/bash

genhtml --function-coverage --branch-coverage --output-directory coverage_html "$(bazel info output_path)/_coverage/_coverage_report.dat"

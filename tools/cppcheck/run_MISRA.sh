#!/bin/bash

set -euo pipefail

mkdir -p reports/cppcheck

# I'm running a custom build of cppcheck with these changes
# https://github.com/cppcheck-opensource/cppcheck/pull/8879
# https://github.com/cppcheck-opensource/cppcheck/pull/8876
# https://github.com/cppcheck-opensource/simplecpp/pull/704
# 
../cppcheck/build/bin/cppcheck \
-P \
-UUDS_LINES \
-UUDS_TP_ISOTP_C_SOCKETCAN \
-UUDS_TP_ISOTP_C_SOCK \
-UUDS_TP_ISOTP_C \
-U_MSC_VER \
-U__GNUC__ \
-DUDS_LOG_LEVEL=UDS_LOG_NONE \
-DUDS_SYS=UDS_SYS_CUSTOM \
iso14229.c \
--platform=unix64 \
--enable=all \
--check-level=exhaustive \
--inconclusive \
--addon=tools/cppcheck/misra.json \
--inline-suppr \
--suppressions-list=tools/cppcheck/suppressions.txt \
--checkers-report=reports/cppcheck/checkers.txt \
2>reports/cppcheck/report_MISRA.txt


# cppcheck \
# --project=compile_commands.json \
# -i src/tp/isotp-c \
# --platform=unix64 \
# --enable=all \
# --addon=tools/cppcheck/misra.json \
# --inline-suppr \
# --suppressions-list=tools/CodeChecker/suppressions.txt \
# --plist-output=cppcheck_reports \
#  -DUDS_SYS=UDS_SYS_UNIX \
# --checkers-report=cppcheck_reports/checkers.txt \
# --library=posix \
# --check-level=exhaustive \
# 2>cppcheck_reports/cppcheck.txt
# --output-format=sarif 2> report.sarif


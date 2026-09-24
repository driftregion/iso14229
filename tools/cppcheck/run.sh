#!/bin/bash
mkdir -p ./cppcheck_reports ./codechecker_cppcheck_reports

../cppcheck/build/bin/cppcheck \
-P \
-UUDS_LINES \
-UUDS_TP_ISOTP_C_SOCKETCAN \
-UUDS_TP_ISOTP_C_SOCK \
-UUDS_TP_ISOTP_C \
-DUDS_LOG_LEVEL=UDS_LOG_NONE \
-DUDS_SYS=UDS_SYS_CUSTOM \
iso14229.c \
--platform=unix64 \
--enable=all \
--addon=tools/cppcheck/misra.json \
--inline-suppr \
--suppressions-list=tools/CodeChecker/suppressions.txt \
--checkers-report=cppcheck_reports/checkers.txt \
2>cppcheck_reports/cppcheck.txt

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


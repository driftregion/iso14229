#!/bin/bash
mkdir -p ./cppcheck_reports ./codechecker_cppcheck_reports


cppcheck \
--project=compile_commands.json \
-i src/tp/isotp-c \
--platform=unix64 \
--enable=all \
--addon=tools/cppcheck/misra.json \
--inline-suppr \
--suppressions-list=tools/CodeChecker/suppressions.txt \
--plist-output=cppcheck_reports \
 -DUDS_SYS=UDS_SYS_UNIX \
--checkers-report=cppcheck_reports/checkers.txt \
--library=posix \
2>cppcheck_reports/cppcheck.txt
# --output-format=sarif 2> report.sarif


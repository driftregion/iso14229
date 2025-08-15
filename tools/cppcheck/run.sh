#!/bin/bash
mkdir -p ./cppcheck_reports ./codechecker_cppcheck_reports


cppcheck \
--project=compile_commands.json \
--platform=unix64 \
--enable=all \
--addon=tools/cppcheck/misra.json \
--suppressions-list=tools/CodeChecker/suppressions.txt \
--plist-output=cppcheck_reports \
 -DUDS_SYS=UDS_SYS_UNIX \
--checkers-report=cppcheck_reports/checkers.txt \
--library=posix \
--output-file=cppcheck_reports/cppcheck.txt \
--file-filter='src/*' \


# report-converter -c -t cppcheck -o ./codechecker_cppcheck_reports ./cppcheck_reports
# CodeChecker parse codechecker_cppcheck_reports --trim-path-prefix '*/cppcheck_reports' > report.txt

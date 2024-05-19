all: release/iso14229.c release/iso14229.h

release/iso14229.c release/iso14229.h:
	mkdir -p release 
	python amalgamate.py --out_c release/iso14229.c --out_h release/iso14229.h

misra.xml: release/iso14229.c release/iso14229.h
	mkdir -p b
	cppcheck  build/iso14229.c build/iso14229.h \
	--cppcheck-build-dir=b \
	--addon=.cppcheck/misra.json \
	--xml 2> misra.xml

misra_report: misra.xml
	cppcheck-htmlreport --file=misra.xml --report-dir=misra --source-dir=.

compile_commands.json:
	bazel run //:host_compile_commands

codechecker_reports: compile_commands.json
	CodeChecker analyze ./compile_commands.json --enable sensitive --output ./codechecker_reports

codechecker: codechecker_reports
	CodeChecker parse ./codechecker_reports

clean:
	rm -rf release misra.xml b misra

.phony: clean misra_report compile_commands.json codechecker

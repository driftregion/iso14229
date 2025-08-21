all: static_analysis

MISRA_RULES_TXT = tools/cppcheck/misra_c_2023__headlines_for_cppcheck.txt

$(MISRA_RULES_TXT):
	mkdir -p tools/cppcheck
	wget -O $(MISRA_RULES_TXT) https://gitlab.com/MISRA/MISRA-C/MISRA-C-2012/tools/-/raw/main/misra_c_2023__headlines_for_cppcheck.txt

compile_commands.json:
	bazel build //:iso14229 && bazel run //:lib_compile_commands

static_analysis: compile_commands.json $(MISRA_RULES_TXT)
	tools/CodeChecker/run.sh

update_srcs:
	tools/update_srcs.sh

coverage:
	tools/canifup.sh
	tools/coverage_run.sh

coverage-html:
	tools/coverage_make_html.sh

coverage-upload:
	./codecov upload-process -t $CODECOV_TOKEN 

.phony: static_analysis compile_commands.json update_srcs coverage coverage-html

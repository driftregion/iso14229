all: static_analysis

MISRA_RULES_TXT = .cppcheck/misra_c_2023__headlines_for_cppcheck.txt

$(MISRA_RULES_TXT):
	mkdir -p .cppcheck
	wget -O $(MISRA_RULES_TXT) https://gitlab.com/MISRA/MISRA-C/MISRA-C-2012/tools/-/raw/main/misra_c_2023__headlines_for_cppcheck.txt

compile_commands.json:
	bazel build //:iso14229 && bazel run //:lib_compile_commands

static_analysis: compile_commands.json $(MISRA_RULES_TXT)
	.CodeChecker/run.sh

update_examples:
	bazel build //:iso14229.c //:iso14229.h 
	cp --update=none bazel-out/k8-fastbuild/bin/iso14229.c bazel-out/k8-fastbuild/bin/iso14229.h -t examples/arduino_server/main
	cp --update=none bazel-out/k8-fastbuild/bin/iso14229.c bazel-out/k8-fastbuild/bin/iso14229.h -t examples/esp32_server/main

.phony: static_analysis compile_commands.json update_examples

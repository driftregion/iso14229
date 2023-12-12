iso14229.c iso14229.h:
	mkdir -p build && bazel build //:iso14229.h //:iso14229.c && cp bazel-bin/iso14229.h bazel-bin/iso14229.c build/

clean:
	rm -rf build

.phony: clean

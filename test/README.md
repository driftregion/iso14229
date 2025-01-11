# Running Tests

```sh
# run tests
bazel test //test:all

# run only tests that don't use vcan
bazel test //test:all --test_tag_filters=-vcan

# run tests and log all output to stdout
bazel test --test_output=all //test:all

# run tests under qemu
bazel test --config=arm_linux //test:all
bazel test --config=ppc //test:all
bazel test --config=ppc64 //test:all
bazel test --config=ppc64le //test:all

# build the fuzzer
bazel build -s --verbose_failures --config=x85_64_clang //test:test_fuzz_server
# run the fuzzer
mkdir .libfuzzer_artifacts .libfuzzer_corpus
bazel-bin/test/test_fuzz_server -jobs=7 -artifact_prefix=./.libfuzzer_artifacts/ .libfuzzer_corpus

# generate coverage
bazel coverage --config=x86_64_gcc -s --combined_report=lcov --instrument_test_targets --instrumentation_filter=^//... --coverage_report_generator=@bazel_tools//tools/test/CoverageOutputGenerator/java/com/google/devtools/coverageoutputgenerator:Main  //test:test_client_sequence

genhtml --branch-coverage --output genhtml "$(bazel info output_path)/_coverage/_coverage_report.dat"
```


# Notes 

If building fails with `/usr/bin/ld: cannot find -lstdc++: No such file or directory`
```sh
sudo apt install libc++-15-dev libc++abi-15-dev
sudo ln -s /usr/lib/x86_64-linux-gnu/libstdc++.so  /usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.30
```

```sh
# If you want to run the qemu tests, you'll need
sudo apt install -y \
qemu-system-arm \
qemu-user \
gcc-arm-linux-gnueabihf \
gcc-powerpc-linux-gnu \
gcc-powerpc64-linux-gnu \
gcc-powerpc64le-linux-gnu \
clang-15 \
```

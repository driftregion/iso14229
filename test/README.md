# Running Tests

```sh
# run tests
bazel test //test:all

# run only tests that don't use vcan
bazel test //test:all --test_tag_filters=-vcan

# run tests and log all output to stdout
bazel test --test_output=all //test:all

# run qemu tests

# ISO-TP linux sockets fail on qemu-arm with the following error:
# ENOPROTOOPT 92 Protocol not available
# however, socketcan linux sockets work fine.
bazel test --config=arm_linux //test:all --test_tag_filters=-isotp_sock
# It seems socketcan is not supported at all on qemu-ppc
bazel test --config=ppc //test:all --test_tag_filters=-vcan
bazel test --config=ppc64 //test:all --test_tag_filters=-vcan
bazel test --config=ppc64le //test:all --test_tag_filters=-vcan

# build the fuzzer
bazel build -s --verbose_failures --config=x86_64_clang //test:test_fuzz_server
# run the fuzzer
mkdir .libfuzzer_artifacts .libfuzzer_corpus
bazel-bin/test/test_fuzz_server -jobs=7 -artifact_prefix=./.libfuzzer_artifacts/ .libfuzzer_corpus

```

## Coverage Testing

The target of coverage testing is the amalgamated source file pair {iso14229.c, iso14229.h}.

```sh
# generate coverage
bazel coverage -s --combined_report=lcov --instrumentation_filter='^//:(iso14229)$'  --experimental_collect_code_coverage_for_generated_files --nocache_test_results //test:test_client_tp_mock //test:test_server_tp_mock //test:test_tp_isotp_compliance_mock

genhtml --branch-coverage --output-directory coverage_html "$(bazel info output_path)/_coverage/_coverage_report.dat"
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

```sh
# static analysis needs
pip3 install codechecker
```


## CI

CircleCI is used to run the coverage tests because it has socketcan support.
The Github Actions Ubuntu runner is not built with socketcan or isotp socket support.
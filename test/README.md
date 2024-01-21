
Testing aims to cover:
- processor architecture (x86, ARM M4, PPC, ...)
- transport layer implementation
- fault injection
- fuzzing
- style


```sh
#!/bin/bash

bazel test --test_output=all --local_test_jobs=1 //test:all

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
# run host tests
bazel test //test:all

# run tests in qemu
bazel test --config=arm_linux //test:all
bazel test --config=ppc //test:all
bazel test --config=ppc64 //test:all
bazel test --config=ppc64le //test:all

# build the fuzzer
bazel build -s --verbose_failures --config=x85_64_clang //test:test_fuzz_server
# run the fuzzer
mkdir .libfuzzer_artifacts .libfuzzer_corpus
bazel-bin/test/test_fuzz_server -jobs=7 -artifact_prefix=./.libfuzzer_artifacts/ .libfuzzer_corpus
```

If building fails with `/usr/bin/ld: cannot find -lstdc++: No such file or directory`

```sh
sudo apt install libc++-15-dev libc++abi-15-dev
sudo ln -s /usr/lib/x86_64-linux-gnu/libstdc++.so  /usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.30
```
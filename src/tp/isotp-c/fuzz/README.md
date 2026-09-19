Fuzz testing
============

This directory contains libFuzzer targets for isotp-c. Fuzzing is optional and
is disabled by default; enable it with `-Disotpc_ENABLE_FUZZING=ON`.

## Safety and scope

The initial target, `isotp_fuzz_receive`, exercises receive-path behaviour only.
It feeds structured sequences into `isotp_on_can_message()`, advances
deterministic mock time, calls `isotp_poll()`, and attempts to drain completed
payloads with `isotp_receive()`.

This target does not currently cover:

* `isotp_send()`-initiated transmission state
* Streaming receive mode
* Callback configurations

Malformed and rejected ISO-TP frames are normal fuzzing outcomes. Failures of
interest include process crashes, ASan findings, UBSan findings, and libFuzzer
timeouts.

## Requirements

Use Linux or a compatible environment with:

* Clang with libFuzzer support
* CMake
* ASan and UBSan support supplied by Clang

The examples below use Ninja as the CMake generator, but the fuzz target is not
tied to Ninja. Enabling fuzzing with an unsupported compiler produces a clear
CMake configuration error.

## Building the receive fuzzer

From the repository root:

```bash
cmake -S . -B build-fuzz \
  -G Ninja \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=Debug \
  -Disotpc_ENABLE_FUZZING=ON

cmake --build build-fuzz --target isotp_fuzz_receive
```

The libFuzzer, ASan, and UBSan flags are applied only to the isolated fuzz
subject and executable, not to the normal library target.

## Smoke run

Run a bounded no-corpus smoke check with:

```bash
./build-fuzz/fuzz/isotp_fuzz_receive -runs=1000 -seed=1
```

`-runs` bounds the number of executions. `-seed` makes the mutation sequence
reproducible.

## Seed corpus

`fuzz/corpus/isotp_receive/` is the source-controlled seed corpus. Do not run
libFuzzer directly against that directory because writable corpus directories may
receive minimized or newly discovered inputs.

Use a disposable copy instead:

```bash
rm -rf build-fuzz/corpus-run
mkdir -p build-fuzz/corpus-run
cp fuzz/corpus/isotp_receive/* build-fuzz/corpus-run/

./build-fuzz/fuzz/isotp_fuzz_receive \
  build-fuzz/corpus-run \
  -runs=5000 \
  -seed=1
```

`build-fuzz/corpus-run/` is disposable and may be mutated by libFuzzer. Newly
generated inputs should not automatically be committed. Only intentional,
minimal regression or coverage seeds should enter the source corpus.

The current receive seed corpus contains:

| Seed | Purpose |
| ---- | ------- |
| `valid_classical_single_frame` | Valid Classical CAN Single Frame followed by a receive attempt. |
| `valid_first_and_consecutive_frame` | Valid First Frame and Consecutive Frame sequence. |
| `wrong_consecutive_frame_sequence` | First Frame followed by a CF with the wrong SN. |
| `receive_timeout` | First Frame, deterministic time advance, poll, and receive attempt. |
| `truncated_single_frame` | Single Frame announcing more data than the frame contains. |
| `invalid_first_frame` | First Frame announcing a payload that should use Single Frame format. |
| `unknown_pci_type` | Frame with an unknown protocol-control-information type. |
| `valid_can_fd_single_frame_escape` | Valid CAN FD Single Frame using the `SF_DL` escape format. |
| `oversized_declared_length` | CAN-frame operation declaring an unsupported length. |

## Longer local fuzzing session

Longer local fuzzing sessions are exploratory and different from a short,
deterministic CI gate. For example:

```bash
./build-fuzz/fuzz/isotp_fuzz_receive \
  build-fuzz/corpus-run \
  -max_total_time=300
```

## Reproducing and triaging findings

Run a saved reproducer with:

```bash
./build-fuzz/fuzz/isotp_fuzz_receive path/to/reproducer
```

A potential finding should be:

1. Reproduced from a clean build
2. Minimized where practical
3. Classified as either a harness problem or library problem
4. Converted into a deterministic unit regression test before changing
   production code

Do not assume every protocol-level anomaly is a security vulnerability.

## Structured input format

Each fuzz input begins with a configuration byte followed by an operation stream:

```text
Byte 0: receive-buffer configuration
Remaining bytes: operation stream
Maximum operations: 32
```

Operation values:

```text
0x00: deliver CAN frame
0x01: advance deterministic microsecond time
0x02: call isotp_poll()
0x03: call isotp_receive()
Other values: no-op
```

Operation encodings:

```text
CAN frame:
0x00 declared_len data_len frame_bytes[data_len]

Time advance:
0x01 delta_u32_le

Poll:
0x02

Receive:
0x03 output_size_selector
```

The harness uses bounded fixed-size storage. CAN-frame operations are copied into
zero-initialized temporary storage before calling the library, so truncated
logical frame data remains malformed protocol input instead of becoming a
harness out-of-bounds access.

# isotp-c

[![CI](https://github.com/SimonCahill/isotp-c/actions/workflows/ci.yml/badge.svg)](https://github.com/SimonCahill/isotp-c/actions/workflows/ci.yml)

isotp-c is a small, platform-independent implementation of ISO 15765-2 (ISO-TP)
for embedded C applications. It segments and reassembles payloads transported
over Classical CAN or CAN FD while leaving CAN I/O, timing, and logging to the
application.

The library is full duplex, performs no dynamic allocation, and supports:

- Classical CAN and CAN FD data lengths up to 64 bytes
- 12-bit and escaped 32-bit First Frame lengths
- Per-link transmit data length (`TX_DL`)
- Polling and optional completion callbacks
- Optional chunked reception for payloads larger than the receive buffer
- Multiple CAN controllers through a per-link user argument
- Optional CAN FD and bit-rate-switch flags for the driver shim

The project was inspired by
[openxc/isotp-c](https://github.com/openxc/isotp-c), but its implementation has
been rewritten.

## Requirements

The library requires a C99 compiler and three application-provided functions:

- `isotp_user_send_can()` sends one CAN or CAN FD frame.
- `isotp_user_get_us()` returns a wrapping, monotonically increasing 32-bit
  microsecond tick.
- `isotp_user_debug()` receives diagnostic messages. It may be a no-op.

No operating system or heap is required. The implementation uses `memcpy`,
`memset`, assertions, and, by default, `snprintf` for two diagnostics. Define
`ISO_TP_NO_FORMATTED_ERRORS` to remove `snprintf`, and use `NDEBUG` in builds
that intentionally disable runtime assertions.

## Quick start

Add `isotp.c` and the public headers to the application, or add this repository
as a CMake subdirectory and link the exported target:

```cmake
add_subdirectory(path/to/isotp-c)
target_link_libraries(my_app PRIVATE simon_cahill::isotp_c)
```

Create one `IsoTpLink` and persistent transmit and receive buffers for each
independent ISO-TP conversation. Initialise it with the CAN identifier used for
outgoing frames:

```c
static IsoTpLink link;
static uint8_t tx_buffer[4095];
static uint8_t rx_buffer[4095];

isotp_init_link(&link, 0x7E0, tx_buffer, sizeof(tx_buffer),
                rx_buffer, sizeof(rx_buffer));
```

Filter received CAN frames in the application and pass frames for this link to
`isotp_on_can_message()`. Call `isotp_poll()` regularly to advance segmented
transmissions and enforce protocol timeouts. Completed messages can then be
removed with `isotp_receive()`:

```c
if (can_id == 0x7E8) {
    isotp_on_can_message(&link, frame_data, frame_size);
}

isotp_poll(&link);

uint32_t received_size;
if (isotp_receive(&link, payload, sizeof(payload), &received_size) == ISOTP_RET_OK) {
    process_message(payload, received_size);
}
```

Send a payload with `isotp_send()`. A successful return means the Single Frame
or First Frame was accepted by the CAN driver. Multi-frame transmission
continues from later calls to `isotp_poll()`; it is not complete merely because
`isotp_send()` returned `ISOTP_RET_OK`.

In a polling-only integration, completion can be observed through
`link.send_status`: it changes from `ISOTP_SEND_STATUS_INPROGRESS` to
`ISOTP_SEND_STATUS_IDLE` on success or `ISOTP_SEND_STATUS_ERROR` on failure.
Inspect `link.send_protocol_result` for the protocol outcome.

See the compile-checked [polling example](https://github.com/SimonCahill/isotp-c/blob/master/examples/isotp_example_polling.c) for
a complete integration skeleton.

### Buffer and call lifetime

- `IsoTpLink` and both buffers must remain valid until the link is destroyed.
- The send buffer must fit the largest payload the application will transmit.
- Without streaming, the receive buffer must fit the entire incoming payload.
  An oversized First Frame is rejected with an Overflow Flow Control frame.
- `isotp_send()` copies the payload before returning, so the caller may reuse
  its source buffer immediately.
- `isotp_receive()` copies at most `payload_size` bytes and then releases the
  message. If the destination is too small, the remainder is discarded.
- Calls operating on the same link must be serialised. Separate links may be
  used independently.

## Platform hooks

The default shim declarations are:

```c
int isotp_user_send_can(uint32_t arbitration_id, const uint8_t* data,
                        uint8_t size);
uint32_t isotp_user_get_us(void);
void isotp_user_debug(const char* message, ...);
```

`isotp_user_send_can()` must return `ISOTP_RET_OK` after accepting a frame,
`ISOTP_RET_NOSPACE` when a transient driver queue condition should be retried,
or `ISOTP_RET_ERROR` for a permanent failure. Its `data` pointer is only valid
during the call.

The microsecond tick may wrap at `UINT32_MAX`; the library's timeout comparisons
account for wraparound. It must advance independently of how often the function
is called.

## Building

### CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The default target is a shared library except on MSVC. For a static library:

```bash
cmake -S . -B build -Disotpc_STATIC_LIBRARY=ON
```

When `isotpc_USE_INCLUDE_DIR=OFF` (the default), include `<isotp.h>`. When it is
enabled, CMake stages the headers beneath an include namespace and consumers
include `<isotp_c/isotp.h>`.

### Make

The native Make build uses the matching uppercase variables from the
configuration table below:

```bash
make all
make USE_STATIC_LIBRARY=ON MAX_CAN_FRAME_SIZE=64 all
```

`make tests` configures and runs the CMake unit suite. `make fuzzing` builds the
libFuzzer receive target with Clang; see the [fuzzing guide](https://github.com/SimonCahill/isotp-c/blob/master/fuzz/README.md).

## Configuration

Use CMake options when the library is a CMake dependency, Make variables with
the native build, or define the corresponding `ISO_TP_*` macros consistently
for both the library and every consumer when compiling the sources directly.

| Capability | CMake | Make | Compile-time definition |
| --- | --- | --- | --- |
| Namespaced staged headers | `isotpc_USE_INCLUDE_DIR` | `USE_INCLUDE_DIR` | — |
| Static library | `isotpc_STATIC_LIBRARY` | `USE_STATIC_LIBRARY` | — |
| PIC static library | `isotpc_STATIC_LIBRARY_PIC` | `ENABLE_STATIC_LIBRARY_PIC` | — |
| Frame padding | `isotpc_PAD_CAN_FRAMES` | `ENABLE_FRAME_PADDING` | `ISO_TP_FRAME_PADDING` |
| Padding byte | `isotpc_CAN_FRAME_PAD_VALUE` | `CAN_FRAME_PAD_VALUE` | `ISO_TP_FRAME_PADDING_VALUE` |
| Maximum CAN data length | `isotpc_MAX_CAN_FRAME_SIZE` | `MAX_CAN_FRAME_SIZE` | `ISO_TP_MAX_CAN_FRAME_SIZE` |
| Initial per-link TX_DL | `isotpc_DEFAULT_TX_DL` | `DEFAULT_TX_DL` | `ISO_TP_DEFAULT_TX_DL` |
| CAN driver user argument | `isotpc_ENABLE_CAN_SEND_ARG` | `ENABLE_CAN_SEND_ARG` | `ISO_TP_USER_SEND_CAN_ARG` |
| CAN frame flags | `isotpc_ENABLE_CAN_SEND_FLAGS` | `ENABLE_CAN_SEND_FLAGS` | `ISO_TP_USER_SEND_CAN_FLAGS` |
| CAN FD bit-rate switch | `isotpc_ENABLE_CAN_FD_BRS` | `ENABLE_CAN_FD_BRS` | `ISO_TP_CAN_FD_USE_BRS` |
| Transmit/receive callbacks | `isotpc_ENABLE_TRANSCEIVE_EVENTS` | `ENABLE_TRANSCEIVE_EVENTS` | callback macros below |
| Chunked receive | `isotpc_ENABLE_STREAMING` | `ENABLE_STREAMING` | `ISO_TP_ENABLE_STREAMING` |
| No formatted diagnostics | `isotpc_NO_FORMATTED_ERRORS` | `NO_FORMATTED_ERRORS` | `ISO_TP_NO_FORMATTED_ERRORS` |

The callback feature can be narrowed with
`isotpc_ENABLE_TRANSMIT_COMPLETE_CALLBACK` and
`isotpc_ENABLE_RECEIVE_COMPLETE_CALLBACK`, or the Make equivalents. Direct
builds use `ISO_TP_TRANSMIT_COMPLETE_CALLBACK` and
`ISO_TP_RECEIVE_COMPLETE_CALLBACK`.

Protocol timing and flow-control defaults can also be overridden before
including the headers: `ISO_TP_DEFAULT_BLOCK_SIZE`,
`ISO_TP_DEFAULT_ST_MIN_US`, `ISO_TP_MAX_WFT_NUMBER`, and
`ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US`.

## CAN FD

Set the compiled maximum CAN data length to one of 12, 16, 20, 24, 32, 48, or
64 bytes. The default is 8-byte Classical CAN:

```bash
cmake -S . -B build-fd -Disotpc_MAX_CAN_FRAME_SIZE=64
```

This maximum affects public structure sizes and must therefore be identical in
the library and its consumers. Each new link initially uses
`ISO_TP_DEFAULT_TX_DL`; select a smaller supported value at runtime when a peer
requires it:

```c
if (isotp_set_tx_dl(&link, 8) != ISOTP_RET_OK) {
    /* Invalid length or a segmented transmission is active. */
}
```

For `TX_DL > 8`, Single Frames use the `SF_DL` escape format when necessary,
First Frames use the full transmit data length, and longer frames are padded to
a legal CAN FD data length. Incoming First Frames establish `RX_DL`, allowing a
CAN FD build to receive both Classical CAN and CAN FD traffic.

Drivers that cannot infer the frame format from its length can enable
`isotpc_ENABLE_CAN_SEND_FLAGS`. The shim then receives `ISOTP_CAN_FRAME_FLAG_FD`
and, when configured, `ISOTP_CAN_FRAME_FLAG_BRS`. If the per-link user argument
is also enabled, the signature is:

```c
int isotp_user_send_can(uint32_t id, const uint8_t* data, uint8_t size,
                        uint8_t flags, void* user_arg);
```

See the compile-checked [CAN FD example](https://github.com/SimonCahill/isotp-c/blob/master/examples/isotp_example_can_fd.c).

## Multiple CAN interfaces

Enable `isotpc_ENABLE_CAN_SEND_ARG` to append a `void*` argument to
`isotp_user_send_can()`. After initialisation, assign the driver or controller
handle to `link.user_send_can_arg`. The value is passed back on every frame sent
for that link. With frame flags enabled, flags precede the user argument.

Incoming arbitration identifiers are intentionally not passed to the library.
The application remains responsible for routing each received frame to the
correct link.

## Streaming receive mode

Enable `isotpc_ENABLE_STREAMING` to receive a message larger than the link's
receive buffer. The library pauses the sender with flow control whenever the
buffer fills. Consume the available chunk with `isotp_receive_streaming()` to
allow reception to continue:

```c
uint32_t chunk_size;
bool complete;

int result = isotp_receive_streaming(&link, chunk, sizeof(chunk),
                                     &chunk_size, &complete);
if (result == ISOTP_RET_OK) {
    store_chunk(chunk, chunk_size, complete);
}
```

The destination must fit the entire currently available chunk. Otherwise the
function returns `ISOTP_RET_NOSPACE` and retains it for a later call. Use the
streaming function consistently for a streaming message; `isotp_receive()`
returns `ISOTP_RET_ERROR` while one is active.

See the compile-checked [streaming example](https://github.com/SimonCahill/isotp-c/blob/master/examples/isotp_example_streaming.c).

## Completion callbacks

Enable `isotpc_ENABLE_TRANSCEIVE_EVENTS` and register callbacks after link
initialisation. A transmit callback runs synchronously from `isotp_send()` for a
Single Frame, or from `isotp_poll()` after the final Consecutive Frame. A
receive callback runs synchronously from `isotp_on_can_message()` when the
message completes.

Registering a receive callback transfers complete-message delivery to the
callback; `isotp_receive()` then returns `ISOTP_RET_ERROR`. The callback data
points into the link's receive buffer and is valid only for the duration of the
callback. Oversized messages still use the streaming API rather than the
receive callback when streaming is enabled.

Callbacks do not remove the need to call `isotp_poll()` for segmented sends and
timeouts. See the compile-checked
[callback example](https://github.com/SimonCahill/isotp-c/blob/master/examples/isotp_example_callbacks.c).

## Functional addressing

`isotp_send_with_id()` overrides the link's configured transmit identifier for
one send and is useful for functional requests. ISO-TP functional addressing is
limited to Single Frames, so the caller must keep the payload within
`ISOTP_SF_MAX_PAYLOAD(isotp_get_tx_dl(&link))`. Use separate links when physical
and functional traffic can have independent receive state.

## Tests, fuzzing, and documentation

Run the unit suite with:

```bash
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Debug -Disotpc_ENABLE_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Compile every integration example with strict warnings:

```bash
cmake -S . -B build-examples -Disotpc_BUILD_EXAMPLES=ON
cmake --build build-examples --target isotpc_examples
```

Generate the Doxygen API reference with `make docs`. The entry point is
`html/index.html`; Doxygen and Graphviz must be installed, and the bundled
stylesheet submodule must be present (`git submodule update --init --recursive`).

## Contributors

See [CONTRIBUTORS.md](https://github.com/SimonCahill/isotp-c/blob/master/CONTRIBUTORS.md) for the people who have contributed to
the project.

## Licence

isotp-c is licensed under the [MIT Licence](https://github.com/SimonCahill/isotp-c/blob/master/LICENSE).

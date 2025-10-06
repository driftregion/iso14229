# iso14229

<p align="center">
<a href="https://github.com/driftregion/iso14229/actions"><img src="https://github.com/driftregion/iso14229/actions/workflows/ci.yml/badge.svg" alt="Build Status"></a>
<a href="https://codecov.io/github/driftregion/iso14229" > 
<img src="https://codecov.io/github/driftregion/iso14229/graph/badge.svg?token=SZP3Q3Y0YE"/> 
</a>
<a href="https://sonarcloud.io/summary/new_code?id=driftregion_iso14229">
    <img src="https://sonarcloud.io/api/project_badges/measure?project=driftregion_iso14229&metric=alert_status">
</a>
<a href="./LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
</p>

iso14229 is an implementation of UDS (ISO14229) targeting embedded systems. It is tested with [`isotp-c`](https://github.com/SimonCahill/isotp-c) as well as [linux kernel](https://github.com/linux-can/can-utils/blob/master/include/linux/can/isotp.h) ISO15765-2 (ISO-TP) transport layer implementations. 

API status: Major version zero (0.y.z) **(not yet stable)**. Anything MAY change at any time.

## Features

- server and client included
- static memory allocation. (no `malloc`, `calloc`, ...)
- highly portable and tested
    - architectures: arm, x86-64, ppc, ppc64, risc
    - systems: linux, Windows, esp32, Arduino, NXP s32k
- multiple transports supported: isotp-c, linux isotp sockets
- examples for esp32, Arduino, NXP S32K144

# Documentation

Here https://driftregion.github.io/iso14229/ and also in [./docs](./docs)

# Contributing

Contributions are welcome.

## Reporting Issues

When reporting issues, please state what you expected to happen.

# Acknowledgements

- [`isotp-c`](https://github.com/SimonCahill/isotp-c) which iso14229 embeds.
- The [NLnet NGI0 Core Fund](https://nlnet.nl/project/iso14229/) which funded some work on iso14229 in 2025.


# Changelog

## 0.9.0
- breaking API changes:
    - converted subfunction enums to #defines with standard-consistent naming
    - simplified transport API
- refined release checklist in #60

## 0.8.0
- breaking API changes:
    - event enum consolidated `UDS_SRV_EVT_...` -> `UDS_EVT`
    - UDSClient refactored into event-based API
    - negative server response now raises a client error by default.
    - server NRCs prefixed with `UDS_NRC_`
    - NRCs merged into `UDS_Err` enum.
- added more examples of client usage


## 0.7.2
- runtime safety:
    1. turn off assertions by default, enable by `-DUDS_ENABLE_ASSERT`
    2. prefer `return UDS_ERR_INVALID_ARG;` over assertion in public functions
- use SimonCahill fork of isotp-c

## 0.7.1
- amalgamated sources into `iso14229.c` and `iso14229.h` to ease integration

## 0.7.0
- test refactoring. theme: test invariance across different transports and processor architectures
- breaking API changes:
    - overhauled transport layer implementation
    - simplified client and server init
    - `UDS_ARCH_` renamed to `UDS_SYS_`

## 0.6.0
- breaking API changes:
    - `UDSClientErr_t` merged into `UDSErr_t`
    - `TP_SEND_INPROGRESS` renamed to `UDS_TP_SEND_IN_PROGRESS`
    - refactored `UDSTp_t` to encourage struct inheritance
    - `UDS_TP_LINUX_SOCKET` renamed to `UDS_TP_ISOTP_SOCKET`
- added server fuzz test and qemu tests
- cleaned up example tests, added isotp-c on socketcan to examples
- added `UDS_EVT_DoScheduledReset`
- improve client error handling

## 0.5.0
- usability: refactored into a single .c/.h module
- usability: default transport layer configs are now built-in
- API cleanup: use `UDS` prefix on all exported functions
- API cleanup: use a single callback function for all server events

## 0.4.0
- refactor RDBIHandler to pass a function pointer that implements safe memmove rather than requiring the user to keep valid data around for an indefinite time or risking a buffer overflow.
- Prefer fixed-width. Avoid using `enum` types as return types and in structures.
- Transport layer is now pluggable and supports the linux kernel ISO-TP driver in addition to `isotp-c`. See [examples](./examples/README.md).

## 0.3.0
- added `iso14229ClientRunSequenceBlocking(...)`
- added server and client examples
- simplified test flow, deleted opaque macros and switch statements
- flattened client and server main structs
- simplified usage by moving isotp-c initialization parameters into server/client config structs 
- remove redundant buffers in server

## 0.2.0
- removed all instances of `__attribute__((packed))`
- refactored server download functional unit API to simplify testing
- refactored tests
    - ordered by service
    - documented macros
- removed middleware 
- simplified server routine control API
- removed redundant function `iso14229ServerEnableService`
- updated example

## 0.1.0
- Add client
- Add server SID 0x27 SecurityAccess
- API changes

## 0.0.0
- initial release


# Cited by

- https://saemobilus.sae.org/papers/comparing-open-source-uds-implementations-fuzz-testing-2024-01-2799

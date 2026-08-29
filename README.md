# iso14229 - UDS Server / Client Library

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

Two files; just works. 

iso14229 is an implementation of UDS (ISO14229) targeting embedded systems. It is tested with [`isotp-c`](https://github.com/SimonCahill/isotp-c) as well as [linux kernel](https://github.com/linux-can/can-utils/blob/master/include/linux/can/isotp.h) ISO15765-2 (ISO-TP) transport layer implementations. 

API status: Major version zero (0.y.z) **(not yet stable)**. Anything MAY change at any time.

## Features:

- includes server and client
- highly portable and tested
    - architectures: arm, x86-64, ppc, ppc64, risc
    - systems: linux, Windows, esp32, Arduino, NXP s32k
    - write your implementation once, it works everywhere
- static memory allocation. (no `malloc`, `calloc`, ...)
- built-in ISO-TP transport support: isotp-c, linux isotp sockets
- examples for esp32, Arduino, NXP S32K144

# Documentation

Here https://driftregion.github.io/iso14229/ and also in [./docs](./docs)

# Contributing

Contributions are welcome.

# Acknowledgements

- [`isotp-c`](https://github.com/SimonCahill/isotp-c) which iso14229 embeds.
- The [NLnet NGI0 Core Fund](https://nlnet.nl/project/iso14229/) which funded some work on iso14229 in 2025.


# Cited by

- https://saemobilus.sae.org/papers/comparing-open-source-uds-implementations-fuzz-testing-2024-01-2799

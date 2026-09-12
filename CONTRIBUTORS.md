|  Name and Profile                                   |  Contribution                                                                                  | PR     |
|-----------------------------------------------------|------------------------------------------------------------------------------------------------|--------|
| [Li Shen](https://github.com/lishen2)               |  Original Author                                                                               |        |
| [Simon Cahill](https://github.com/SimonCahill)      |  CMake support, documentation improvements, code simplification, fork owner                    |        |
| [Brandon Ros](https://github.com/brandonros)        |  Migrated timestamps from milliseconds to microseconds.                                        | #12    |
| [Nick James Kirkby](https://github.com/driftregion) |  Fixed include path warnings on Win64                                                          | #13    |
| [Mark A Clark](https://github.com/maclark88)        |  Fixed documentation inconsistencies                                                           | #18    |
| [speedy-h](https://github.com/speedy-h)             |  Fixed compile- and link-time options                                                          | #23    |
| [Phil Greenland](https://github.com/pgreenland)     |  Added padding support in ISOTP messages                                                       | #24    |
| [Phil Greenland](https://github.com/pgreenland)     |  Added support for message retries if the remote controller was busy                           | #26    |
| [Phil Greenland](https://github.com/pgreenland)     |  Fixed PIC being generated for static binaries                                                 | #27    |
| [speedy-h](https://github.com/speedy-h)             |  Fixed compiler error when compiling with `-Wextra` caused by unused function params.          | #32    |
| [kazetsukaimiko](https://github.com/kazetsukaimiko) |  Added support for PlatformIO, a platform for distributing libraries for a variety of systems. | #35    |
| [driftregion](https://github.com/driftregion)       |  Added support for optional CAN arguments for adapter-specific information.                    | #36    |
| [andyneubacher](https://github.com/AndyNeubacher)   |  Added support for larger frame sizes (ISO15765-2:2016).                                       | #46    |
| [mws262](https://github.com/mws262)                 |  Fixed snprintf format string for embedded devices (replaced %d w/ %u)                         | #47    |
| [eternal-echo](https://github.com/eternal-echo)     |  Fixed minor issues with conditional compilation regarding user-args to can_send.              | #50    |
| [markxoe](https://github.com/markxoe)               |  ISOTP settings are now configurable at build time.                                            | #54    |
| [speedy-h](https://github.com/speedy-h)             |  Don't propagate link option to dependents                                                     | #63    |
| [94xhn](https://github.com/94xhn)                   |  Fix MinGW/GCC-on-Windows build broken by _WIN32 vs __GNUC__ macro clash                       | #65    |
| [mccre110](https://github.com/mccre110)             |  Added support for CAN-FD.                                                                     | #69    |
| [jerry73204](https://github.com/jerry73204)         |  Added ISO_TP_NO_FORMATTED_ERRORS to drop the snprintf dependency.                              | #75    |
| [Max Proskauer](https://github.com/mproskauer-cmt) |  Fixed `isotp_send_with_id()` ignoring the identifier for Single Frames.                       | #79    |

Thank you everyone for contributing to this library and improving it!
Have you contributed and I've forgotten to mention you? Please let me know and I'll add you here!

## Add DoIP (ISO 13400) Transport Layer Support

This PR implements DoIP (Diagnostic over IP - ISO 13400) as a new transport layer for UDS, enabling diagnostic communication over TCP/IP networks.

### Features

- **DoIP Client Implementation** (`src/tp/doip/`)
  - TCP-based diagnostic message transport (ISO 13400-2)
  - Routing activation and alive check handling
  - Proper DoIP message framing and state management
  - Support for diagnostic message ACK/NACK
  - Non-blocking I/O with configurable timeouts

### Implementation Details

- **Protocol Version**: DoIP v3 (ISO 13400:2019)
- **Standard Port**: TCP 13400
- **Addressing**: External test equipment range (0x0E00-0x0FFF)
- **Message Types**: Routing activation, diagnostic messages, alive check

### Testing

- Compiles cleanly with `-Wall -Wpedantic -Wextra`
- Integration test passes (`examples/doip_client/test.sh`)
- Successfully exchanges RDBI/WDBI messages over DoIP

### Code Quality

- Consistent with iso14229 coding style
- Comprehensive error handling and logging
- Well-documented with inline comments and function documentation
- No compiler warnings

### Missing Features

- UDP vehicle discovery
- DoIP server implementation in `src/tp/doip/` (currently only in examples)
- TLS support (port 3496)
- Unit tests for DoIP module
- Multi-client server support

### Related Standards

- ISO 13400-2:2019 - Road vehicles — Diagnostic communication over Internet Protocol (DoIP)
- ISO 14229 - Unified Diagnostic Services (UDS)

---

**Testing Instructions:**

```bash
cd examples/doip_client
make
[test.sh](http://_vscodecontentref_/3)  # Automated integration test
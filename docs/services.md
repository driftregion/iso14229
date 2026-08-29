# UDS Services {#services}

This page documents the services supported by iso14229.

## Supported Services {#supported-services}

| Service ID | Service Name | Server | Client | Standard Responses* |
|------------|--------------|--------|--------|----------------------|
| 0x10 | \ref service_0x10 "Diagnostic Session Control" | Y | Y | \ref service_0x10_supported_responses "NRCs" |
| 0x11 | \ref service_0x11 "ECU Reset" | Y | Y | \ref service_0x11_supported_responses "NRCs" |
| 0x14 | \ref service_0x14 "Clear Diagnostic Information" | Y | N | \ref service_0x14_supported_responses "NRCs" |
| 0x19 | Read DTC Information | N | N | |
| 0x22 | \ref service_0x22 "Read Data By Identifier" | Y | Y | \ref service_0x22_supported_responses "NRCs" |
| 0x23 | Read Memory By Address | N | N | |
| 0x24 | Read Scaling Data By Identifier | N | N | |
| 0x27 | \ref service_0x27 "Security Access" | Y | Y | \ref service_0x27_supported_responses "NRCs" |
| 0x28 | \ref service_0x28 "Communication Control" | Y | Y | \ref service_0x28_supported_responses "NRCs" |
| 0x2A | Read Periodic Data By Identifier | N | N | |
| 0x2C | Dynamically Define Data Identifier | N | N | |
| 0x2E | \ref service_0x2e "Write Data By Identifier" | Y | Y | \ref service_0x2e_supported_responses "NRCs" |
| 0x2F | Input/Output Control By Identifier | Y | N | |
| 0x31 | \ref service_0x31 "Routine Control" | Y | Y | \ref service_0x31_supported_responses "NRCs" |
| 0x34 | \ref service_0x34 "Request Download" | Y | Y | \ref service_0x34_supported_responses "NRCs" |
| 0x35 | Request Upload | Y | Y | |
| 0x36 | \ref service_0x36 "Transfer Data" | Y | Y | \ref service_0x36_supported_responses "NRCs" |
| 0x37 | \ref service_0x37 "Request Transfer Exit" | Y | Y | \ref service_0x37_supported_responses "NRCs" |
| 0x38 | Request File Transfer | Y | Y | |
| 0x3D | Write Memory By Address | Y | N | |
| 0x3E | Tester Present | Y | Y | |
| 0x83 | Access Timing Parameter | N | N | |
| 0x84 | Secured Data Transmission | N | N | |
| 0x85 | Control DTC Setting | Y | Y | |
| 0x86 | Response On Event | N | N | |
| 0x87 | Link Control | Y | N | |

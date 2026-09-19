# Security Policy

## Supported Versions

Security updates are provided for the latest released minor version of isotp-c. Users should upgrade to the latest patch release before reporting a problem.

| Version | Supported          |
| ------- | ------------------ |
| 1.7.x   | :white_check_mark: |
| 1.6.x   | :white_check_mark: |
| < 1.6   | :x:                |

The `master` branch contains the latest stable development state and receives security fixes alongside supported releases.

## Reporting a Vulnerability

Please report suspected security vulnerabilities through [GitHub Private Vulnerability Reporting](https://github.com/SimonCahill/isotp-c/security/advisories/new).

Do not disclose the vulnerability in a public GitHub issue, pull request, discussion, or other public forum before a fix is available.

Include as much of the following information as possible:

- A description of the vulnerability and its potential impact
- The affected version, platform, and compiler
- The configuration options used to build the library
- Steps or sample code that reproduce the issue
- Relevant CAN frames, logs, traces, or crash reports
- Any suggested mitigation or fix
- Whether the vulnerability has been disclosed elsewhere

You can normally expect:

- Acknowledgement within 7 days
- An initial assessment within 14 days
- Periodic updates while an accepted report is being investigated
- Coordination before publication of an advisory or fix

If the report is accepted, we will work to reproduce the issue, determine the affected versions, prepare a fix, and coordinate disclosure. Credit will be given in the advisory when requested.

If the report is declined, we will explain why it is not considered a security vulnerability or why it falls outside the scope of this project. Reports that cannot be reproduced may require additional information before a final decision can be made.

Please allow a reasonable remediation period before publicly disclosing an accepted vulnerability.

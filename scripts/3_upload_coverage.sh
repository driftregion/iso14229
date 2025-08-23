#!/bin/bash

set -euxo pipefail

# curl -Os https://cli.codecov.io/latest/linux/codecov
# chmod +x codecov
./codecov upload-process -t $CODECOV_TOKEN -F fuzz

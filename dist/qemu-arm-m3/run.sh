#!/bin/bash

set -euo pipefail

app="${1:-build/deploy/bin/1_helloworld}"
shift || true

qemu-system-arm -machine netduino2 -s -nographic -kernel "${app}" "$@"


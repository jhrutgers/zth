#!/bin/bash

set -euo pipefail

app="${1:-build/deploy/bin/1_helloworld}"
shift || true

qemu-system-arm -machine virt -cpu cortex-a15 -s -m 8M -nographic -kernel "${app}" "$@"


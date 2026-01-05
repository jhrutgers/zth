#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

app="${1:-build/deploy/bin/1_helloworld}"
shift || true

qemu-system-arm -machine virt -cpu cortex-a15 -s -m 8M -nographic -semihosting -kernel "${app}" "$@"


#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

app="${1:-build/deploy/bin/1_helloworld}"
shift || true

debugging=0
if [[ " $* " == *" -S "* ]]; then
	debugging=1
fi

if [[ ${debugging} -eq 1 ]]; then
	echo "Waiting for GDB connection on port 1234 to debug ${app}..." >&2
	set +e
fi

qemu-system-arm -machine virt -cpu cortex-a15 -s -m 8M -nographic -semihosting -kernel "${app}" "$@"
res=$?

if [[ ${debugging} -eq 1 ]]; then
	set -e
	echo "QEMU exited with status ${res}" >&2
fi

exit ${res}

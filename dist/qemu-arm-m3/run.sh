#!/bin/bash

# Zth (libzth), a cooperative userspace multitasking library.
# Copyright (C) 2019-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

set -euo pipefail

app="${1:-build/deploy/bin/1_helloworld}"
shift || true

qemu-system-arm -machine netduino2 -s -nographic -semihosting -kernel "${app}" "$@"


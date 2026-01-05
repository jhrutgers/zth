#!/bin/bash

# SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)" > /dev/null

[[ ! -z ${CC:-} ]]  || CC=gcc
[[ ! -z ${CXX:-} ]] || CXX=g++

cmake_opts=
. ../common/build.sh

popd > /dev/null

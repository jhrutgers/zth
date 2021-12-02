#!/bin/bash

set -e

pushd "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null; pwd -P)" > /dev/null

[[ ! -z ${CC:-} ]]  || CC=gcc
[[ ! -z ${CXX:-} ]] || CXX=g++

. ../common/build.sh

popd > /dev/null

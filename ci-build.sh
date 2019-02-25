#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset

cd /hlwm
./ci-check-using-std.sh

mkdir build
cd build

export CC=gcc-8 CXX=g++-8
linkerfix="-fuse-ld=gold"
export LDFLAGS="$linkerfix" LDXXFLAGS="$linkerfix"

cmake -GNinja -DWITH_DOCUMENTATION=NO -DCMAKE_BUILD_TYPE=Debug ..
ninja -v -k 10

tox -e py37 -- -n auto -v

#!/bin/bash

###
# Checks source with include-what-you-use by running cmake in a temporary build
# folder to obtain a compile database.
###

set -o errexit
set -o nounset
set -o pipefail

iwyu_ver=$(include-what-you-use --version | awk '{print $2}')
iwyu_ver_maj=$(echo "$iwyu_ver" | cut -d. -f 1)
iwyu_ver_min=$(echo "$iwyu_ver" | cut -d. -f 2)

if ! (( iwyu_ver_maj > 0 || iwyu_ver_min >= 11 )); then
    echo >&2 "Error: Expecting include-what-you-use version >= 0.11, got $iwyu_ver"
    exit 1
fi

tmpdir=$(mktemp -d)
trap '{ rm -rf "$tmpdir"; }' EXIT

srcdir=$PWD
pushd "$tmpdir"

###
# Check #include order
###
fix_include --dry_run --sort_only --reorder "$srcdir"/src/*.h

###
# Actually run include-what-you-use
###
export CXX=clang++-7 CC=clang-7 LDXX=clang++-7 LD=clang-7
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$srcdir"
iwyu_tool -p . -j "$(nproc)" > iwyu.log

if [[ -s iwyu.log ]]; then
    echo >&2 "Error: include-what-you-use has the following change requests:"
    cat >&2 iwyu.log

    # Do not make it fail yet:
    # exit 1
fi

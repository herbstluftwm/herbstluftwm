#!/bin/bash

###
# Checks source with include-what-you-use by running cmake in a temporary build
# folder to obtain a compile database.
###

set -o pipefail
set -o nounset
set -o xtrace

tmpdir=$(mktemp -d)
trap "{ rm -rf $tmpdir; }" EXIT

srcdir=$PWD
pushd $tmpdir
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$srcdir"

iwyu_tool.py -p . -j $(nproc) > iwyu.log

if [[ -s iwyu.log ]]; then
    echo >&2 "IWYU (include-what-you-use) has the following change requests:"
    cat >&2 iwyu.log
    exit 1
fi

cat >&2 iwyu.log

#!/bin/bash

###
# This script is a wrapper around clang and clang-tidy. It behaves mostly like
# clang (or clang++, depending on the program name), but also executes
# clang-tidy on the given source file.
#
# The purpose of this is to reduce CI check times by tricking ccache into
# serving as a cache for clang-tidy as well. The base assumption for this is
# inputs for clang-tidy match those of the compiler, so a ccache hit implies
# that this file has passed the clang-tidy checks before.
#
# For simplicity, this script makes a couple of assumptions, that hold within
# the our controlled CI environment:
#   * there is only ever one source file per compiler invocation
#   * the source file is always the last argument
#   * the source file name matches *.cpp or *.c
#   * the compile_commands.json in the build dir corresponds the current build
#   * clang-tidy gets its config string passed via -config=
#   * when switching the version of clang or clang-tidy, this script is also
#     modified (otherwise ccache wouldn't detect the compiler change)
###

set -o errexit
set -o nounset

# Perform the regular compiler invocation:
progname=$(basename "$0")
if [[ $progname == clang-* ]]; then
    clang-14 "$@"
elif [[ $progname == clang++-* ]]; then
    clang++-14 "$@"
else
    echo >&2 "Error: Cannot handle program name: $progname"
    exit 1
fi

# Extract the source file from the call (simply assume it is the last
# argument):
sourcefile=${BASH_ARGV[0]}

# Make sure it is a call for compilation:
if [[ $sourcefile != *.cpp && $sourcefile != *.c ]]; then
    exit 0
fi

if [[ ! -v CLANG_TIDY_BUILD_DIR ]]; then
    # This is probably a sanity check invocation by cmake, so we should not run
    # clang-tidy.
    exit 0
fi

###
# All clang-tidy arguments and its configuration must be defined within this
# script, so that ccache can detect changes to the configuration.
###
checks=""
function add_check() {
    checks=${checks:+$checks,}$1
}

add_check readability-braces-around-statements
add_check google-readability-braces-around-statements
add_check hicpp-braces-around-statements
add_check readability-container-size-empty
add_check bugprone-macro-parentheses
add_check bugprone-bool-pointer-implicit-conversion
add_check bugprone-suspicious-string-compare
add_check cppcoreguidelines-pro-type-member-init
# Sadly, this check gets easily confused by templates and reports false
# positives. Check with newer clang-tidy versions in the future:
#add_check readability-inconsistent-declaration-parameter-name

clang_tidy_args="-header-filter=.* -extra-arg=-Wno-unknown-warning-option -p=${CLANG_TIDY_BUILD_DIR} -config={} -warnings-as-errors=* -checks=$checks"

# Run clang-tidy, but hide its output unless it fails (non-failing stdout
# confuses ccache!)
if ! out="$(clang-tidy-14 $clang_tidy_args "$sourcefile" 2>&1)"; then
    echo "$out"
    exit 1
fi

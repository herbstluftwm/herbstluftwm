#!/bin/bash

###
# Searches the code for the use of "std::" prefixes in symbol reference that
# should be pulled into the namespace with "using std::…;".
#
# The list of symbols to enforce this for is assembled on-the-fly by looking
# for occurrences of "using std::…;"
###

set -o pipefail
set -o nounset

reporoot=$(realpath "$(dirname "$0")")

# Assemble list of std symbols to pull in:
usethis=$(grep -r --no-filename 'using std::' "$reporoot/src/"*.cpp \
    | sed -r 's/^\s*using std::(.*);/\1/' \
    | sort -u)

# Fall back to hardcoded list until the rule is actually enforced:
usethis="string vector"

found_something=0
for symbol in $usethis; do
    # Find offending occurences of "std::" prefixes:
    grep -r --color=auto "[^(using ) ]std::$symbol" src/*.cpp
    grepret=$?
    if [[ $grepret == 0 ]]; then
        found_something=1
    fi
done

if [[ $found_something != 0 ]]; then
    echo >&2 "Found redundant uses of std:: prefixes (see above)"
    exit 1
fi

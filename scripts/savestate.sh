#!/usr/bin/env bash

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}

# prints a machine readable format of all tags and its layouts
# one tag with its layout per line

# a common usage is:
# savestate.sh > mystate
# and sometime later:
# loadstate.sh < mystate

hc complete 1 use |
while read tag ; do
    echo -n "$tag: "
    hc dump "$tag"
done

#!/usr/bin/env bash

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}

# loads layouts for each tag coming from stdin
# the format is the one created by savestate.sh

# a common usage is:
# savestate.sh > mystate
# and sometime later:
# loadstate.sh < mystate

while read line ; do
    tag="${line%%: *}"
    tree="${line#*: }"
    hc add "$tag"
    hc load "$tag" "$tree"
done

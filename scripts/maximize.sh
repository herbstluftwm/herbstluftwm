#!/bin/bash -e
# A simple script for window maximization and window switching.
# Running this the first time script will:
#
#  1. remember the current layout
#  2. squeeze all windows into one frame using the layout defined in the first
#     argument (defaulting to max layout).
#  3. and during that, keeping the window focus
#
# Running this script again will:
#
#  1. restore the original layout
#  2. (again keeping the then current window focus)
#
# If you call this script with "grid", then you obtain a window switcher,
# similar to that of Mac OS X.
mode=${1:-max} # just some valid layout algorithm name

# FIXME: for some unknown reason, remove_attr always fails
#        fix that and remove the "try" afterwards
layout=$(herbstclient dump)
cmd=(
substitute FOCUS clients.focus.winid chain
. lock
. or : and , silent substitute STR tags.focus.my_unmaximized_layout load STR
           , try remove_attr tags.focus.my_unmaximized_layout
     : chain , new_attr string tags.focus.my_unmaximized_layout
             , set_attr tags.focus.my_unmaximized_layout "$layout"
             , load "(clients $mode:0 )"
. jumpto FOCUS
. unlock
)
herbstclient "${cmd[@]}"

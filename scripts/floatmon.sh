#!/usr/bin/env bash

monitor=floatmon
tag=fl
Mod=${Mod:-Mod1}
Floatkey=${Floatkey:-Shift-f}

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}

if which xwininfo &> /dev/null; then
    size=$(xwininfo -root |
           sed -n -e '/^  Width: / {
                          s/.*: //;
                          h
                      }
                      /^  Height: / {
                          s/.*: //g;
                          H;
                          x;
                          s/\n/x/p
                      }')
else
    echo "This script requires the xwininfo binary."
    exit 1
fi

hc chain , add "$tag" , floating "$tag" on
hc or , add_monitor "$size"+0+0 "$tag" "$monitor" \
      , move_monitor "$monitor" "$size"+0+0
hc raise_monitor "$monitor"
hc lock_tag "$monitor"

cmd=(
or  case: and
        # if not on floating monitor
        . compare monitors.focus.name != "$monitor"
        # and if a client is focused
        . get_attr clients.focus.winid
        # then remember the last monitor of the client
        . chain try new_attr string clients.focus.my_lastmon
                try true
        . substitute M monitors.focus.index
            set_attr clients.focus.my_lastmon M
        # and then move the client to the floating tag
        . shift_to_monitor "$monitor"
        . focus_monitor "$monitor"
        . true
    case: and
        # if on the floating monitor
        . compare monitors.focus.name = "$monitor"
        # and if a client is focused
        . get_attr clients.focus.winid
        # then send it back to the original monitor
        . substitute M clients.focus.my_lastmon chain
            , shift_to_monitor M
            , focus_monitor M
        . true
    case: and
        # if the previous things fail,
        # just move to the first monitor
        . shift_to_monitor 0
        . focus_monitor 0
)

hc keybind $Mod-$Floatkey "${cmd[@]}"


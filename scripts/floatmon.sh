#!/bin/bash

monitor=floatmon
tag=fl
Mod=${Mod:-Mod1}
Floatkey=${Floatkey:-Shift-f}

hc() { herbstclient "$@" ; }

size=$(xwininfo -root |
                grep -E '^  (Width|Height):' |
                cut -d' ' -f4 |
                sed 'N;s/\n/x/')

hc chain , add $tag , floating $tag on
hc or , add_monitor "$size"+0+0 $tag $monitor \
      , move_monitor $monitor "$size"+0+0
hc raise_monitor $monitor
hc lock_tag $monitor

cmd=(
or  case: and
        # if not on floating monitor
        . compare monitors.focus.name != $monitor
        # and if a client is focused
        . get_attr clients.focus.winid
        # then remember the last monitor of the client
        . chain try new_attr string clients.focus.my_lastmon
                try true
        . substitute M monitors.focus.index
            set_attr clients.focus.my_lastmon M
        # and then move the client to the floating tag
        . shift_to_monitor $monitor
        . focus_monitor $monitor
        . true
    case: and
        # if on the floating monitor
        . compare monitors.focus.name = $monitor
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

herbstclient keybind $Mod-$Floatkey "${cmd[@]}"


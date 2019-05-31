#!/usr/bin/env bash

# a q3-like (or yakuake-like) terminal for arbitrary applications.
#
# this lets a new monitor called "q3terminal" scroll in from the top into the
# current monitor. There the "scratchpad" will be shown (it will be created if
# it doesn't exist yet). If the monitor already exists it is scrolled out of
# the screen and removed again.
#
# Warning: this uses much resources because herbstclient is forked for each
# animation step.
#
# If a tag name is supplied, this is used instead of the scratchpad

tag="${1:-scratchpad}"
hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}

termwidth_percent=${WIDTH_PERC:-100}
mrect=( $(hc monitor_rect -p "" ) )
termwidth=$(( (${mrect[2]} * termwidth_percent) / 100 ))
termheight=${HEIGHT_PIXELS:-400}

rect=(
    $termwidth
    $termheight
    $(( ${mrect[0]} + (${mrect[2]} - termwidth) / 2 ))
    $(( ${mrect[1]} - termheight ))
)

y_line=${mrect[1]}


hc add "$tag"


monitor=q3terminal

exists=false
if ! hc add_monitor $(printf "%dx%d%+d%+d" "${rect[@]}") \
                    "$tag" $monitor 2> /dev/null ; then
    exists=true
else
    # remember which monitor was focused previously
    hc chain \
        , new_attr string monitors.by-name."$monitor".my_prev_focus \
        , substitute M monitors.focus.index \
            set_attr monitors.by-name."$monitor".my_prev_focus M
fi

update_geom() {
    local geom=$(printf "%dx%d%+d%+d" "${rect[@]}")
    hc move_monitor "$monitor" $geom
}

steps=${ANIMATION_STEPS:-5}
interval=${ANIMATION_INTERVAL:-0.01}

animate() {
    progress=( "$@" )
    for i in "${progress[@]}" ; do
        rect[3]=$((y_line - (i * termheight) / steps))
        update_geom
        sleep "$interval"
    done
}

show() {
    hc lock
    hc raise_monitor "$monitor"
    hc focus_monitor "$monitor"
    hc unlock
    hc lock_tag "$monitor"
    animate $(seq $steps -1 0)
}

hide() {
    rect=( $(hc monitor_rect "$monitor" ) )
    local tmp=${rect[0]}
    rect[0]=${rect[2]}
    rect[2]=${tmp}
    local tmp=${rect[1]}
    rect[1]=${rect[3]}
    rect[3]=${tmp}
    termheight=${rect[1]}
    y_line=${rect[3]} # height of the upper screen border

    animate $(seq 0 +1 $steps)
    # if q3terminal still is focused, then focus the previously focused monitor
    # (that mon which was focused when starting q3terminal)
    hc substitute M monitors.by-name."$monitor".my_prev_focus \
        and + compare monitors.focus.name = "$monitor" \
            + focus_monitor M
    hc remove_monitor "$monitor"
}

[ $exists = true ] && hide || show


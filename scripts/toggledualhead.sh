#!/usr/bin/env bash

# Splits the currently focused monitor into two monitors displayed side by side
# Running this on a splitted monitor joins the two monitor halfs again.

hc() {
    herbstclient "$@"
}

array2rect() {
    printf "%dx%d%+d%+d" $3 $4 $1 $2
}

idx=$(hc get_attr monitors.focus.index)

if orig=$(hc get_attr monitors.${idx}.my_orig_rect 2> /dev/null ) ; then
    # give original size and remove all other monitors without the leader flag
    rect=$(array2rect $orig)
    mon_cnt=$(hc get_attr monitors.count)
    cmd=(
        chain
            X move_monitor $idx "$rect"
            X remove_attr monitors.${idx}.my_orig_rect
            X or
    )
    for ((i=0 ; i < mon_cnt ; i++ )) ; do
        # find the other monitor half and remove it
        [ $i != $idx ] &&
        cmd+=( v and
                    ∧ compare monitors.${i}.my_orig_rect = "${orig[*]}"
                    ∧ remove_monitor $i
        )
    done
    hc "${cmd[@]}" > /dev/null 2> /dev/null
else
    # split original rectangle of the monitor into a left and a right half
    orig=( $(hc monitor_rect $i) ) || exit 1
    left=( ${orig[0]} ${orig[1]} $((${orig[2]} / 2)) ${orig[3]} )
    x=$(( ${left[0]} + ${left[2]} ))
    rightwidth=$((${orig[2]} - ${left[2]}))
    right=( $x ${orig[1]} $rightwidth ${orig[3]} )
    leftrect=$(array2rect ${left[@]})
    rightrect=$(array2rect ${right[@]})
    hc chain \
        , lock                                                      \
        , new_attr string monitors.${idx}.my_orig_rect              \
        , set_attr monitors.${idx}.my_orig_rect "${orig[*]}"        \
        , move_monitor ${idx} "$leftrect"                           \
        , sprintf ATTR "monitors.%s.my_orig_rect" monitors.count    \
            chain                                                   \
            . add_monitor "$rightrect"                              \
            . new_attr string ATTR                                  \
            . set_attr ATTR "${orig[*]}"                            \
        , unlock
fi

# restart the panels
herbstclient emit_hook quit_panel

panelcmd=${panelcmd:-~/.config/herbstluftwm/panel.sh}
if ! [ "$panelcmd" ] ; then
    # fall back to global panel if there is no user-specific panel
    panelcmd=/etc/xdg/herbstluftwm/panel.sh
fi

for monitor in $(herbstclient list_monitors | cut -d: -f1) ; do
    # start it on each monitor
    "$panelcmd" $monitor &
done



#!/bin/bash

monitor=${1:-0}
height=16
font="fixed"
bgcolor='#3E2600'

herbstclient pad $monitor $height
(
    # events:
    #mpc idleloop player &
    while true ; do
        date +'date ^fg(#efefef)%H:%M^fg(#909090), %Y-%m-^fg(#efefef)%d'
        sleep 1 || break
    done &
    herbstclient --idle
)|(
    TAGS=( $(herbstclient tag_status $monitor) )
    date=""
    while true ; do
        bordercolor="#26221C"
        hintcolor="#573500"
        separator="^fg(#141414)^ro(1x$height)^fg()"
        # draw tags
        for i in "${TAGS[@]}" ; do
            case ${i:0:1} in
                '#')
                    echo -n "^bg(#9fbc00)^fg(#141414)"
                    ;;
                '+')
                    echo -n "^bg(#9CA668)^fg(#141414)"
                    ;;
                *)
                    echo -n "^bg(#6A4100)^fg()"
                    ;;
            esac
            echo -n " ${i:1} "
            echo -n "$separator"
        done
        echo -n "^bg()^p(_CENTER)"
        # small adjustments
        width=140
        echo -n "^p(_RIGHT)^p(-$width)$separator^bg($hintcolor) $date $separator"
        echo
        # wait for next event
        read line || break
        cmd=( $line )
        # find out event origin
        case "${cmd[0]}" in
            tag*)
                #echo "reseting tags" >&2
                TAGS=( $(herbstclient tag_status $monitor) )
                ;;
            date)
                #echo "reseting date" >&2
                date="${cmd[@]:1}"
                ;;
            #player)
            #    ;;
        esac
    done
) |dzen2 -fn "$font" -h $height -xs $((monitor+1)) \
    -ta l -bg "$bgcolor" -fg '#efefef'





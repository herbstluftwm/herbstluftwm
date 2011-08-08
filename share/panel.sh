#!/bin/bash

font="Bitstream Vera Sans Mono-10:Bold"
monitor=${1:-0}
height=14

herbstclient pad $monitor $height
(
    #mpc idleloop &
    herbstclient --idle
)|(
    TAGS=( $(herbstclient tag_status $monitor) )
    active=true
    cur=
    for x in ${!TAGS[@]} ; do
        i=${TAGS[$x]}
        if [ "${i:0:1}" = '*' ] ; then
            cur="${i:1}"
            TAGS[$x]=${i:1}
            break;
        fi
        if [ "${i:0:1}" = '+' ] ; then
            cur="${i:1}"
            TAGS[$x]=${i:1}
            active=false
            break;
        fi
    done
    while true ; do
        echo -n "^fg()^bg()"
        for t in "${TAGS[@]}" ; do
            [ "$t" = $cur ] && echo -n "^bg(#9fbc00)^fg(#141414)"
            echo -n " $t ^fg()^bg()"
        done
        echo
        # wait for next event
        read i || break
        cmd=( $i )
        case "${cmd[0]}" in
            tag_changed)
                if [ "${cmd[2]}" != $monitor ] ; then
                    active=false
                fi
                if [ "${cmd[2]}" = $monitor ] ; then
                    cur="${cmd[1]}"
                    active=true
                fi
                ;;
        esac
    done
) |dzen2 -h $height -xs $((monitor+1)) -ta l -fn "$font" -bg '#34291C' -fg '#FFEF80'





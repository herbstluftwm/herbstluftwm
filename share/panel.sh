#!/bin/bash

font="Bitstream Vera Sans Mono-10:Bold"
monitor=${1:-0}
height=14

herbstclient pad $monitor $height
(
    #mpc idleloop &
    herbstclient --idle
)|(
    while true ; do
        tag=$(herbstclient tag_status $monitor \
            |sed 's.\([^\t+*]\{1,\}\).^ca(1, herbstclient use \1) \1 ^ca().g' \
            |sed 's.\t[+].^bg(#FFE0BB)^fg(#141414).g' \
            |sed 's.\t[*].^bg(#9fbc00)^fg(#141414).g' \
            |sed 's.\t.^bg()^fg().g' \
            )
        echo "$tag"

        # wait for next event
        read $i || break
    done
) |dzen2 -h $height -xs $((monitor+1)) -ta l -fn "$font" -bg '#34291C' -fg '#FFEF80'





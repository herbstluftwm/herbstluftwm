#!/bin/bash

monitor=${1:-0}
geometry=( $(herbstclient monitor_rect "$monitor") )
if [ -z "$geometry" ] ;then
    echo "Invalid monitor $monitor"
    exit 1
fi
# geometry has the format: WxH+X+Y
x=${geometry[0]}
y=${geometry[1]}
width=${geometry[2]}
height=16
font="-*-fixed-medium-*-*-*-12-*-*-*-*-*-*-*"
bgcolor='#3E2600'

function uniq_linebuffered() {
    awk '$0 != l { print ; l=$0 ; fflush(); }' "$@"
}

herbstclient pad $monitor $height
{
    # events:
    #mpc idleloop player &
    while true ; do
        date +'date ^fg(#efefef)%H:%M^fg(#909090), %Y-%m-^fg(#efefef)%d'
        sleep 1 || break
    done > >(uniq_linebuffered)  &
    childpid=$!
    herbstclient --idle
    kill $childpid
} 2> /dev/null | {
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
                ':')
                    echo -n "^bg(#6A4100)^fg(#141414)"
                    ;;
                '!')
                    echo -n "^bg(#FF0675)^fg(#141414)"
                    ;;
                *)
                    echo -n "^bg()^fg()"
                    ;;
            esac
            echo -n '^ca(1, herbstclient use "'${i:1}'") '"${i:1} ^ca()"
            echo -n "$separator"
        done
        echo -n "^bg()^p(_CENTER)"
        # small adjustments
        right="$separator^bg($hintcolor) $date $separator"
        right_text_only=$(echo -n "$right"|sed 's.\^[^(]*([^)]*)..g')
        # get width of right aligned text.. and add some space..
        width=$(textwidth "$font" "$right_text_only  ")
        echo -n "^p(_RIGHT)^p(-$width)$right"
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
            quit_panel)
                exit
                ;;
            #player)
            #    ;;
        esac
        done
} 2> /dev/null | dzen2 -w $width -x $x -y $y -fn "$font" -h $height \
    -ta l -bg "$bgcolor" -fg '#efefef'





#!/bin/bash


panelcmd=${1:-$(dirname $0)/panel.sh}

killall $(basename $panelcmd)
for i in $(herbstclient list_monitors|cut -d':' -f1) ; do
    $panelcmd $i &
done


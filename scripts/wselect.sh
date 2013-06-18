#!/bin/bash

# a window selection utility
# dependencies: wmctrl, awk,
#               dmenu with multiline support (command line flag -l)

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}
dm() { "${dmenu_command[@]:-dmenu}" "$@" ;}
dmenu_lines=${dmenu_lines:-10}

case "$1" in

    bring)
        # bring the selected window to the current tag and focus it
        name=Bring:
        action() { hc bring "$@" ; }
        ;;

    select|*)
        # switch to the selected window and focus it
        action() { hc jumpto "$@" ; }
        name=Select:
        ;;
esac

id=$(wmctrl -l |cat -n| sed 's/\t/) /g'| sed 's/^[ ]*//' \
    | dm -l $dmenu_lines -p "$name") \
    && action $(awk '{ print $2 ; }' <<< "$id")

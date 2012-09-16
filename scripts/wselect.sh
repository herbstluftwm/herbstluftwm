#!/bin/bash

# a window selection utility
# dependences: xdotool, wmctrl,
#              dmenu with multiline support (command line flag -l)

dmenu_command=${dmenu_command:-dmenu}
dmenu_lines=${dmenu_lines:-10}

case "$1" in

    bring)
        # bring the selected window to the current tag and focus it
        name=Bring:
        action() {
            herbstclient bring "$@"
        }
        ;;

    select|*)
        # switch to the selected window and focus it
        action() { xdotool windowactivate "$@" ; }
        name=Select:
        ;;
esac

id=$(wmctrl -l | $dmenu_command -l $dmenu_lines -p "$name") \
    && action ${id%% *}


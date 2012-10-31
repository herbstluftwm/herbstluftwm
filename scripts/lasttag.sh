#!/bin/bash

# usage: start this script in anywhere your autostart (but *after* the
# emit_hook reload line)

# to switch to the last tag, call: herbstclient emit_hook goto_last_tag
# or bind it: herbstclient keybind Mod1-Escape emit_hook goto_last_tag

herbstclient --idle '(tag_changed|goto_last_tag|reload)' \
    | while read line ; do
        ARGS=( $line )
        case ${ARGS[0]} in
            tag_changed)
                LASTTAG="$TAG"
                TAG=${ARGS[1]}
                ;;
            goto_last_tag)
                ! [ -z "$LASTTAG" ] && herbstclient use "$LASTTAG"
                ;;
            reload)
                exit
                ;;
        esac
    done

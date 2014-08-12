#!/usr/bin/env bash

# usage: start this script in anywhere your autostart (but *after* the
# emit_hook reload line)

# to switch to the last tag, call: herbstclient emit_hook goto_last_tag
# or bind it: herbstclient keybind Mod1-Escape emit_hook goto_last_tag

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}
hc --idle '(tag_changed|goto_last_tag|reload)' \
    | while read line ; do
        IFS=$'\t' read -ra args <<< "$line"
        case ${args[0]} in
            tag_changed)
                lasttag="$tag"
                tag=${args[1]}
                ;;
            goto_last_tag)
                [ "$lasttag" ] && hc use "$lasttag"
                ;;
            reload)
                exit
                ;;
        esac
    done

#!/usr/bin/env bash

# herbstcommander.sh - launch herbstluftwm-commands via dmenu
# Written by Florian Bruhin <me@the-compiler.org>

# To customize dmenu-colors, create a file named "herbstcommander" in your
# herbstluftwm config-directory, with something like this in it:
#
# dmenu_command=(dmenu -i -b -nb '#313131' -nf '#686868' -sb '#454545' -sf '#898989')
#
# You can also directly pass dmenu-arguments to this script instead, as long
# as dmenu_command is undefined.

config_1="$XDG_CONFIG_HOME/herbstluftwm/herbstcommander"
config_2="$HOME/.config/herbstluftwm/herbstcommander"
[[ -f "$config_1" ]] && source "$config_1"
[[ -f "$config_2" ]] && source "$config_2"

dm() {
    if [[ "${dmenu_command[@]}" ]]; then
        "${dmenu_command[@]}" "$@"
    else
        dmenu -i "$@"
    fi
}

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}
prompt=${prompt:-herbstluft: }
display_reply=${display_reply:-true}

cmd=( "$@" )
forceexec=0

while :; do
    dmenu_args=""
    if [[ "$forceexec" != 1 ]]; then
        completion=$(hc complete "${#cmd[@]}" "${cmd[@]}")
        if [[ "$?" = 7 ]] ; then
            forceexec=1
        fi
    fi
    if [[ "$forceexec" == 1 ]]; then
        echo "Executing ${cmd[@]}"
        reply=$(hc "${cmd[@]}")
        status=$?
        if [[ "$display_reply" && "$reply" ]]; then
            dm -p "${cmd[*]}" <<< "$reply" >/dev/null
        fi
        exit $status
    else
        case "${cmd[*]}" in
            raise|jumpto|bring)
                IFS=$'\t' read -ra tags <<< "$(hc tag_status)"
                i=1
                completion=$(
                    wmctrl -l | while read line; do
                        IFS=' ' read -ra fields <<< "$line"
                        id=${fields[0]}
                        tag=${tags[ ${fields[1]} ]}
                        class=$(xprop -notype -id $id WM_CLASS |
                            sed 's/.*\?= *//; s/"\(.*\?\)", *"\(.*\?\)".*/\1,\2/')
                        title=${fields[@]:3}
                        printf "%-3s %s %-3s [%s] %s\n" "$i)" "$id" "$tag" "$class" "$title"
                        i=$((i+1))
                    done
                )

                dmenu_args="-l 10"
                ;;
        esac
        next=$(dm $dmenu_args -p "${prompt}${cmd[*]}" <<< "$completion")
        (( $? != 0 )) && exit 125 # dmenu was killed
        if [[ -z "$next" ]]; then
            forceexec=1 # empty reply instead of completion
        else
            case "${cmd[*]}" in
                raise|jumpto|bring)
                    # add the WINID only (second field)
                    IFS=' ' read -ra fields <<< "$next"
                    cmd+=( ${fields[1]} )
                    ;;
                *)
                    cmd+=( $next )
                    ;;
            esac
        fi
    fi
done

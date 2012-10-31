#!/bin/bash

# herbstcommander.sh - launch herbstluftwm-commands via dmenu
# Written by Florian Bruhin <me@the-compiler.org>

# To customize dmenu-colors, create a file named "herbstcommander" in your
# herbstluftwm config-directory, with something like this in it:
#
# dmenu_cmd="dmenu -i -b -nb #313131 -nf #686868 -sb #454545 -sf #898989"
#
# You can also directly pass dmenu-arguments to this script instead, as long
# as dmenu_cmd is undefined.

config_1="$XDG_CONFIG_HOME/herbstluftwm/herbstcommander"
config_2="$HOME/.config/herbstluftwm/herbstcommander"
[[ -f "$config_1" ]] && source "$config_1"
[[ -f "$config_2" ]] && source "$config_2"

dmenu_cmd=${dmenu_cmd:-dmenu -i}
herbstclient_cmd=${herbstclient_cmd:-herbstclient}
prompt=${prompt:-herbstluft: }
display_reply=${display_reply:-true}

cmd=( "$@" )
forceexec=0

while :; do
    dmenu_args=""
    if [[ "$forceexec" != 1 ]]; then
        completion=$($herbstclient_cmd complete "${#cmd[@]}" "${cmd[@]}")
        if [[ "$?" = 7 ]] ; then
            forceexec=1
        fi
    fi
    if [[ "$forceexec" == 1 ]]; then
        echo "Executing ${cmd[@]}"
        reply=$($herbstclient_cmd "${cmd[@]}")
        status=$?
        if [[ "$display_reply" && "$reply" ]]; then
            $dmenu_cmd -p "${cmd[*]}" <<< "$reply" >/dev/null
        fi
        exit $status
    else
        case "${cmd[*]}" in
            raise|jumpto|bring)
                tags=( $($herbstclient_cmd tag_status) )
                i=1
                completion=$(
                    wmctrl -l | while read line; do
                        fields=( $line )
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
        next=$($dmenu_cmd $dmenu_args -p "${prompt}${cmd[*]}" <<< "$completion")
        (( $? != 0 )) && exit 125 # dmenu was killed
        if [[ -z "$next" ]]; then
            forceexec=1 # empty reply instead of completion
        else
            case "${cmd[*]}" in
                raise|jumpto|bring)
                    # add the WINID only (second field)
                    fields=( $next )
                    cmd+=( ${fields[1]} )
                    ;;
                *)
                    cmd+=( $next )
                    ;;
            esac
        fi
    fi
done

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

dmenu_cmd=${dmenu_cmd:-dmenu $@}
herbstclient_cmd=${herbstclient_cmd:-herbstclient}
prompt=${prompt:-herbstluft: }
display_reply=${display_reply:-true}

cmd=()
forceexec=0

while :; do
	if [[ "$forceexec" != 1 ]]; then
		completion=$($herbstclient_cmd complete "${#cmd[@]}" "${cmd[@]}")
	fi
	if [[ -z "$completion" || "$forceexec" == 1 ]]; then
		echo "Executing ${cmd[@]}"
		reply=$($herbstclient_cmd "${cmd[@]}")
		status=$?
		if [[ "$display_reply" && "$reply" ]]; then
			 $dmenu_cmd -p "${cmd[*]}" <<< "$reply" >/dev/null
		fi
		exit $status
	else
		next=$($dmenu_cmd -p "${prompt}${cmd[*]}" <<< "$completion")
		(( $? != 0 )) && exit 125 # dmenu was killed
		if [[ -z "$next" ]]; then
			forceexec=1 # empty reply instead of cmpletion
		else
			cmd+=( $next )
		fi
	fi
done

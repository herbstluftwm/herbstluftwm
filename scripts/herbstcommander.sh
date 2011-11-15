#!/bin/bash

# herbstcommander.sh - launch herbstluftwm-commands via dmenu
# Written by Florian Bruhin <me@the-compiler.org>

config="$XDG_CONFIG_HOME/herbstluftwm/herbstcommander"
[[ -f "$config" ]] && source "$config"
dmenu_cmd=${dmenu_cmd:-dmenu}
herbstclient_cmd=${herbstclient_cmd:-herbstclient}
prompt=${prompt:-herbstluft: }
display_reply=${display_reply:-true}

cmd=()

while :; do
	completion=$(herbstclient complete "${#cmd[@]}" "${cmd[@]}") 
	if [[ -z "$completion" ]]; then
		echo "Executing ${cmd[@]}"
		reply=$(herbstclient "${cmd[@]}")
		status=$?
		if [[ "$display_reply" && "$reply" ]]; then
			 dmenu -p "${cmd[*]}" <<< "$reply" >/dev/null
		fi
		exit $status
	else
		cmd+=( $(dmenu -p "${prompt}${cmd[*]}" <<< "$completion") )
		(( $? != 0 )) && exit 125 # dmenu was killed
	fi
done

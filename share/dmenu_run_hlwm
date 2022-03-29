#!/usr/bin/env bash

if ! command -v dmenu > /dev/null 2>/dev/null ; then
	echo "Error: Requirement dmenu not found in your PATH." >&2
	exit 1
fi

font=$(herbstclient attr theme.title_font)
bgcolor=$(herbstclient get frame_border_normal_color |sed 's,^\(\#[0-9a-f]\{6\}\)[0-9a-f]\{2\}$,\1,')
selbg=$(herbstclient get window_border_active_color  |sed 's,^\(\#[0-9a-f]\{6\}\)[0-9a-f]\{2\}$,\1,')
selfg='#101010'
FLAGS=( -nb "$bgcolor" -sb "$selbg" -sf "$selfg" )

case "$font" in
  -*-*-*-*-*-*-*-*-*-*-*-*-*-*)
    # ignore XLFD fonts because the current dmenu only supports XFT.
    ;;
  *)
    FLAGS+=( -fn "$font" )
    ;;
esac

# Get the currently active tag
tag=$(herbstclient attr tags.focus.name)

# Redirect to dmenu_path if available
if command -v dmenu_path > /dev/null 2>/dev/null ; then
        selectedPath=$(dmenu_path | dmenu "${FLAGS[@]}" "$@")

# If at least stest is present use the code from latest dmenu_path directly
elif command -v stest > /dev/null 2>/dev/null ; then
	cachedir=${XDG_CACHE_HOME:-"$HOME/.cache"}
	if [ -d "$cachedir" ]; then
		cache=$cachedir/dmenu_run
	else
		cache=$HOME/.dmenu_cache # if no xdg dir, fall back to dotfile in ~
	fi
	IFS=:
	if stest -dqr -n "$cache" $PATH; then
		selectedPath=$(stest -flx $PATH | sort -u | tee "$cache" | dmenu "${FLAGS[@]}" "$@")
	else
		selectedPath=$(dmenu "${FLAGS[@]}" "$@" < "$cache")
	fi

# Both not found -> unable to continue
else
	echo "Error: Requirements dmenu_path or stest not found in your PATH." >&2
	exit 2
fi

# Stop here if the user aborted
[ -z $selectedPath ] && exit 0

# Move next window from this process to this tag. Prepend the rule so
# that it may be overwritten by existing custom rules e.g. in the
# autostart. Also set a maximum age for this rule of 120 seconds and
# mark it as one-time-only rule.
herbstclient rule prepend maxage="120" pid="$$" tag="$tag" once

exec $selectedPath

#!/usr/bin/env bash

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}

tag="$1"
expire="120" # expiry time in seconds
shift

if [ -z "$1" ] ;then
    echo "usage: $0 TAG COMMAND [ARGS ...]" >&2
    echo "executes a COMMAND on a specific TAG" >&2
    echo "if TAG doesnot exist, it will be created" >&2
    echo "if TAG is empty, current tag will be used" >&2

fi

tag=${tag:-$(hc attr tags.focus.name)}

# ensure tag exists
hc add "$tag"

# move next window from this process to this tag
# prepend the rule so that it may be overwritten by existing custom rules e.g.
# in the autostart
hc rule prepend maxage="$expire" pid="$$" tag="$tag" once

exec "$@"

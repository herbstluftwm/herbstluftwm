#!/bin/bash


TAG="$1"
EXPIRE="120" # expiry time in seconds
shift

if [ -z "$1" ] ;then
    echo "usage: $0 TAG COMMAND [ARGS ...]" >&2
    echo "executes a COMMAND on a specific TAG" >&2
    echo "if TAG doesnot exist, it will be created" >&2
    echo "if TAG is empty, current tag will be used" >&2

fi

hc() {
    herbstclient "$@"
}

curtag() {
     hc tag_status \
        | grep -oE "$(echo -ne '\t')#[^$(echo -ne '\t')]*" \
        | tail -c +3
}

TAG=${TAG:-$(curtag)}

# ensure tag exists
hc add "$TAG"

# move next window from this process to this tag
hc rule maxage="$EXPIRE" pid="$$" tag="$TAG" once

exec "$@"


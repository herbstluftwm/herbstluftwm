#!/bin/bash


TAG="$1"
EXPIRE="120" # expiry time in seconds
shift

if [ -z "$TAG" ] || [ -z "$1" ] ;then
    echo "usage: $0 TAG COMMAND [ARGS ...]" >&2
    echo "executes a COMMAND on a specific TAG" >&2
    echo "if TAG doesnot exist, it will be created" >&2

fi

function hc() {
    herbstclient "$@"
}

# ensure tag exists
hc add "$TAG"

# move next window from this process to this tag
hc rule maxage="$EXPIRE" pid="$$" tag="$TAG" once

exec "$@"


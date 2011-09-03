#!/bin/bash

dmenu_command=${dmenu_command:-dmenu}
hc=${herbstclient_command:-herbstclient}

dmenu_cmd() {
    $dmenu_command "$@"
}

simple_command() {
    arg=$($hc complete 1 "$@"|dmenu_cmd -p "$@:") \
    && exec $hc "$@" "$arg"
}

case "$1" in
    use|move)    simple_command "$1" ;;
    *)
        echo "unknown menu $1" >&2
        ;;
esac





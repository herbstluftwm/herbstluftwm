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

simple_command "$1"


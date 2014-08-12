#!/usr/bin/env bash

dm() { "${dmenu_command[@]:-dmenu}" "$@" ;}
hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}

simple_command() {
    arg=$(hc complete 1 "$@" | dm -p "$@:") \
    && exec "${herbstclient_command[@]:-herbstclient}" "$@" "$arg"
}

simple_command "$1"

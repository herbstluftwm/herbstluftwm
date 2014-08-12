#!/usr/bin/env bash

# exec a script $2... with settings from rc-file $1
# useful for various dmenu scripts, e.g.:
# ./execwith.sh ~/.bash_settings ./dmenu.sh use

source "$1"
shift
exec "$@"

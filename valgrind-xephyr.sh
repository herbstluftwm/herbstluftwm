#!/usr/bin/env bash

# run a herbstluftwm with valgrind in a xephyr
# run this from a build directory, i.e. directory in which you run make.

die() {
    echo "$*" >&2
    exit 1
}

set -e

cmdname="$0"
usage() {
cat <<EOF
Usage: $cmdname FLAGS

Start a Xephyr and run herbstluftwm in it. Per default, valgrind
is used. Other debug environments are:

    --valgrind          Run in valgrind (default)
    --gdb               Run in gdb
    --none              Run directly
    --autostart=FILE    Use the autostart FILE
    -h --help           Print this help
EOF
}
AUTOSTART=""
debugger() {
    valgrind "$@"
}

for arg in "$@" ; do
    case "$arg" in
        --valgrind)
            ;;
        --gdb)
            debugger() {
                gdb -ex run --args "$@"
            }
            ;;
        --vim-debug)
            debugger() {
                echo "+TermdebugCommand $@" | xargs -I{} -0 vim '{}'
            }
            ;;
        --none)
            debugger() {
                "$@"
            }
            ;;
        --autostart=*)
            AUTOSTART=${arg#--autostart=}
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument $arg"
            usage >&2
            exit 1
            ;;
    esac
done

# find free display number
# ------------------------
xephyr_displaynr= # only the display number without the colon
for displaynr in {11..80} ; do
    if [[ ! -e /tmp/.X11-unix/X$displaynr ]] ; then
        xephyr_displaynr="$displaynr"
        break
    fi
done

if [[ -z "$xephyr_displaynr" ]] ; then
    die "No free display found"
fi

# set up herbstluftwm
# -------------------
export PATH="$(pwd):$PATH"
if [[ -z "$AUTOSTART" ]] ; then
    # if AUTOSTART hasn't specified yet
    project_dir=$(dirname "$0")
    project_dir=${project_dir%/*}
    AUTOSTART="$project_dir"/share/autostart
fi

# boot up Xephyr
# --------------
Xephyr -resizeable ":$xephyr_displaynr" &
xephyr_pid=$!

while sleep 0.3; do
    echo "Waiting for display :$xephyr_displaynr to appear"
    if [[ -e /tmp/.X11-unix/X$xephyr_displaynr ]] ; then
        break
    fi
done

# boot up herbstluftwm
# --------------------
DISPLAY=":$xephyr_displaynr" \
    debugger ./herbstluftwm --verbose -c "$AUTOSTART" || die "Warning: debugger had non-zero exit code"

# clean up xephyr
kill "$xephyr_pid"

#!/usr/bin/env bash

# run a herbstluftwm with valgrind in a xephyr

die() {
    echo "$*" >&2
    exit 1
}

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
# TODO: force the current herbstclient to be used?

# don't let glib cause valgrind errors
export G_SLICE=always-malloc
export G_DEBUG=gc-friendly
DISPLAY=":$xephyr_displaynr" \
    valgrind -- \
        ./herbstluftwm --verbose -c ./share/autostart

# clean up xephyr
kill "$xephyr_pid"

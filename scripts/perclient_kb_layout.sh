#!/usr/bin/env bash

# Preserves keyboard layout when switching between windows.
# Handy if you, for example, use your native language in a messenger app,
# do some coding in an editor (Vim especially), and switch back and forth.
# Messenger stays Russian, Vim stays English.

# === Usage ===
#
# Put this script next to your `autostart` (~/.config/herbstluftwm/).
# Make sure it is executable:
#
#     chmod +x ~/.config/herbstluftwm/perclient_kb_layout.sh
#
# Install xkblayout-state (<https://github.com/nonpop/xkblayout-state>).
#
# Somewhere in your `autostart` add:
#
#     pkill -u $USER --full perclient_kb_layout
#     $(dirname "$0")/perclient_kb_layout.sh &


if ! command -v xkblayout-state &> /dev/null
then
    echo >&2 "$0 requires xkblayout-state to be on \$PATH";
    echo >&2 "Grab it from: https://github.com/nonpop/xkblayout-state";
    echo >&2 "BTW, if using Arch: https://aur.archlinux.org/packages/xkblayout-state-git/";
    exit 1;
fi

hc() {
    herbstclient "$@"
}

FOCUS_WINID=$(hc attr clients.focus.winid)

hc --idle focus_changed | while read hook winid name
do
    # Save current keyboard layout for window loosing focus
    hc try silent new_attr int clients.${FOCUS_WINID}.my_kb_layout;
    hc silent attr clients.${FOCUS_WINID}.my_kb_layout "$(xkblayout-state print '%c')";

    # Save the currently focused win id to be able referring it as the one loosing focus
    FOCUS_WINID=$winid

    # Restore previously stored layout.
    # Fallback to the default (0'th) if the window is new and so has no stored attribute.
    # Redirect stderr to /dev/null to suppress:
    #     Object "clients.focus" has no attribute "my_kb_layout"
    # ..in this case
    xkblayout-state set $(hc attr clients.focus.my_kb_layout 2>/dev/null || echo 0);
done;

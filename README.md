===== HERBSTLUFTWM =====

Copyright 2011-2013 Thorsten Wißmann. All rights reserved.

This software is licensed under the "Simplified BSD License".
See LICENSE for details.

==== Requirements ====
Build dependencies:
    - build-environment (gcc/other compiler, make)
    - asciidoc (only when building from git, not when building from tarball)
    - a posix system with _POSIX_TIMERS and _POSIX_MONOTONIC_CLOCK or a system
      with a current mach kernel
Runtime dependencies:
    - bash (if you use the default autostart file)
    - glib >= 2.14
    - libx11

Optional run-time dependencies:
    - xsetroot (to set wallpaper color in default autostart)
    - xterm (used as the terminal in default autostart)
    - dzen2 (used in the default panel.sh, it works best with a new dzen2 which
             already supports clicking)
    - dmenu (used in some example scripts)

==== Help/Support/Bugs ====
A list of known bugs is listed in BUGS. If you found other bugs or want to
request features then contact the mailing list. (The subscription process is
explained in the HACKING file).

Mailing list: hlwm@lists.herbstluftwm.org

For instant help join the IRC channel: #herbstluftwm on irc.freenode.net

==== Steps with installing ====
If you are using a system with a package manager, then install it via the
package manager of your distribution! If you are not allowed to install
software, then contact your system administrator.

You only need to install it manually if you do not like package managers or if
you are creating a package for your distribution.

The compilation and installation is configured by the following make-variables
in config.mk:

DESTDIR = /                     # the path to your root-directory
PREFIX = /usr/                  # the prefix
SYSCONFDIR = $(DESTDIR)/etc/    # path to etc directory

Normally you should build it with DESTDIR=/ and install it with
DESTDIR=./path/to/fakeroot if you are building a package.

    make DESTDIR=/
    sudo make DESTDIR=./build/ install
    mkdir -p ~/.config/herbstluftwm/
    cp /etc/xdg/herbstluftwm/autostart ~/.config/herbstluftwm/autostart

==== First steps without installing ====
1. compile it:

    make

2. copy herbstclient to a bin-folder or adjust path in autostart file
3. copy default autostart file to the config-dir:

    mkdir -p ~/.config/herbstluftwm
    cp share/autostart ~/.config/herbstluftwm/

4. add the share/herbstclient-completion to your /etc/bash_completion.d/ folder
   or source it in your bashrc
5. run it in a session that has no windowmanager yet

==== Starting it ====
Start it within a running X-session with:

    herbstluftwm --locked

The --locked causes herbstluftwm not to update the screen until you unlock it
with: herbstclient unlock (This is done automatically by the default autostart)

==== Quirks ====
Mac OSX:

Problem: Mod1 is nowhere to be found.
Solution: Set left Command (Apple) key to be Mod1.
edit .Xmodmap
--- snip ---
! Make the Alt/Option key be Alt_L instead of Mode_switch
keycode 63 = Alt_L

! Make Meta_L be a Mod4 and get rid of Mod2
clear mod2
clear mod4
add mod4 = Meta_L

! Make Alt_L be a Mod1
clear mod1
add mod1 = Alt_L
--- snap ---

herbstluftwm
============

image:https://github.com/herbstluftwm/herbstluftwm/workflows/HLWM%20CI/badge.svg[link=
https://github.com/herbstluftwm/herbstluftwm/actions?query=workflow%3A%22HLWM+CI%22]
image:https://codecov.io/gh/herbstluftwm/herbstluftwm/branch/master/graph/badge.svg[link=
https://codecov.io/gh/herbstluftwm/herbstluftwm]

herbstluftwm is a manual tiling window manager for X. It licensed under the
"Simplified BSD License" (see link:LICENSE[LICENSE]).

- the layout is based on splitting frames into subframes which can be split
  again or can be filled with windows (similar to i3/ musca)

- tags (or workspaces or virtual desktops or …) can be added/removed at
  runtime. Each tag contains an own layout

- exactly one tag is viewed on each monitor. The tags are monitor independent
  (similar to xmonad)

- it is configured at runtime via ipc calls from herbstclient. So the
  configuration file is just a script which is run on startup. (similar to
  wmii/musca)

For more, see the http://herbstluftwm.org[herbstluftwm homepage] -- in
particular the http://herbstluftwm.org/tutorial.html[herbstluftwm tutorial]
for the first steps (also available as `man herbstluftwm-tutorial` after
installing herbstluftwm on your system).

You are welcome to join the IRC channel `#herbstluftwm` on `irc.libera.chat`.

Installation
------------
If you want to build herbstluftwm from source, see the link:INSTALL[INSTALL] file.

// vim: ft=asciidoc tw=80

#!/usr/bin/env bash

# A script allowing to minimize and un-minimize clients in a LIFO way
# (last minimized client will be un-minimized first).
# 
# `chmod +x unminimize.sh` then call it or add it to `autostart`.


Mod=${Mod:-Mod4}
Minimizekey=Shift-m
Unminimizekey=Ctrl-m
# get the absolute path of this script, to call it when minimizing
SCRIPT_PATH=$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/$(basename "${BASH_SOURCE[0]}")

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}


# 
# initialize minimize and unminimize shortcuts
#
init() {

   # initialize a global minimization counter
   hc silent new_attr uint my_minimized_counter 1

   # minimize current window
   hc keybind $Mod-$Minimizekey spawn "$SCRIPT_PATH" minimize

   # unminimize last window of a tag
   # if the `my_minimized_age` attribute does not exist (i.e. the window has not been
   #  minimized with this script), use arbitrary order to unminimize
   hc keybind $Mod-$Unminimizekey mktemp string LASTCLIENTATT mktemp uint LASTAGEATT chain \
     . set_attr LASTAGEATT 0 \
     . foreach CLIENT clients. and \
       , sprintf MINATT "%c.minimized" CLIENT \
           compare MINATT "=" "true" \
       , sprintf TAGATT "%c.tag" CLIENT substitute FOCUS "tags.focus.name" \
           compare TAGATT "=" FOCUS \
       , sprintf AGEATT "%c.my_minimized_age" CLIENT or \
         case: and \
            : ! get_attr AGEATT \
            : compare LASTAGEATT "=" 0 \
         case: and \
            : substitute LASTAGE LASTAGEATT \
                compare AGEATT 'gt' LASTAGE \
            : substitute AGE AGEATT \
                set_attr LASTAGEATT AGE \
       , set_attr LASTCLIENTATT CLIENT \
     . and \
       , compare LASTCLIENTATT "!=" "" \
       , substitute CLIENT LASTCLIENTATT chain \
         : sprintf MINATT "%c.minimized" CLIENT \
             set_attr MINATT false \
         : sprintf AGEATT "%c.my_minimized_age" CLIENT \
             try remove_attr AGEATT \

}


# 
# minimize focused client
#
minimize() {

   hc and \
     . substitute C my_minimized_counter new_attr uint clients.focus.my_minimized_age C \
     . set_attr my_minimized_counter $(($(hc get_attr my_minimized_counter)+1)) \
     . set_attr clients.focus.minimized true \

}


if [ "$1" = "minimize" ] ; then minimize ; else init ; fi

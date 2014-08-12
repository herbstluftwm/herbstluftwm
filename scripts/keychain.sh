#!/usr/bin/env bash

# Execute this (e.g. from your autostart) to obtain basic key chaining like it
# is known from other applications like screen.
#
# E.g. you can press Mod1-i 1 (i.e. first press Mod1-i and then press the
# 1-button) to switch to the first workspace
#
# The idea of this implementation is: If one presses the prefix (in this case
# Mod1-i) except the notification, nothing is really executed but new
# keybindings are added to execute the actually commands (like use_index 0) and
# to unbind the second key level (1..9 and 0) of this keychain. (If you would
# not unbind it, use_index 0 always would be executed when pressing the single
# 1-button).

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}
Mod=Mod1

# Create the array of keysyms, the n'th entry will be used for the n'th
# keybinding
keys=( {1..9} 0 )

# Build the command to unbind the keys
unbind=(  )
for k in "${keys[@]}" Escape ; do
    unbind+=( , keyunbind "$k" )
done

# Add the actual bind, after that, no new processes are spawned when using that
# key chain. (Except the spawn notify-send of course, which can be deactivated
# by only deleting the appropriate line)

hc keybind $Mod-i chain \
    '->' spawn notify-send "Select a workspace number or press Escape" \
    '->' keybind "${keys[0]}" chain "${unbind[@]}" , use_index 0 \
    '->' keybind "${keys[1]}" chain "${unbind[@]}" , use_index 1 \
    '->' keybind "${keys[2]}" chain "${unbind[@]}" , use_index 2 \
    '->' keybind "${keys[3]}" chain "${unbind[@]}" , use_index 3 \
    '->' keybind "${keys[4]}" chain "${unbind[@]}" , use_index 4 \
    '->' keybind "${keys[5]}" chain "${unbind[@]}" , use_index 5 \
    '->' keybind "${keys[6]}" chain "${unbind[@]}" , use_index 6 \
    '->' keybind "${keys[7]}" chain "${unbind[@]}" , use_index 7 \
    '->' keybind "${keys[8]}" chain "${unbind[@]}" , use_index 8 \
    '->' keybind "${keys[9]}" chain "${unbind[@]}" , use_index 9 \
    '->' keybind Escape       chain "${unbind[@]}"

#!/bin/bash

# aligns the output of dump command as a nice tree
# usage:
# herbstclient dump | ./dumpbeatify.sh

indent=""

# an ugly hack, but it's okey:
IFS="" # we do not need fields
# but with IFS="" read -n 1 can read spaces :)

function p(){
    while read -n 1 p ; do
        [ "$p" = '(' ] && echo -en '\n'"$indent"'(' && indent+=' ' && ( p ) && continue
        [ "$p" = ')' ] && echo -en ')' && return 0
        echo -n "$p"
    done
    echo
}

p | sed '/^[ ]*$/d'


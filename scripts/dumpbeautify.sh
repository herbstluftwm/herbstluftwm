#!/usr/bin/env bash

# aligns the output of dump command as a nice tree
# usage:
# herbstclient dump | ./dumpbeatify.sh

awkcode='
BEGIN {
    indent=2
    ORS=""
    x=0
    open=0
    first=1
    i=0
    color[i++]="\033[1;31m"
    color[i++]="\033[1;32m"
    color[i++]="\033[1;33m"
    color[i++]="\033[1;34m"
    color[i++]="\033[1;35m"
    color[i++]="\033[1;36m"
    color[i++]="\033[1;37m"
    color[i++]="\033[0;31m"
    color[i++]="\033[0;32m"
    color[i++]="\033[0;33m"
    color[i++]="\033[0;34m"
    color[i++]="\033[0;35m"
    color[i++]="\033[0;36m"
    color[i++]="\033[0;37m"
}

$1 ~ "^[(]" {
    if (first == 0) {
        printf "\n"
        printf "%"(indent*x)"s" , ""
    } else {
        first=0
    }
    color_bracket[x]=open
    print color[(color_bracket[x]) % length(color)]
    gsub("[(]", "&" clear)
    print
    x++
    open++
}

$1 ~ "[)]" {
    x--
    print color[(color_bracket[x]) % length(color)]
    print
}

END {
    printf clear "\n"
}
'

clear=$(tput sgr0) || clear=$(echo -e '\e[0m')

sed 's/[()]/\n&/g' | # insert newlines before (
    awk -v "clear=$clear" "$awkcode"

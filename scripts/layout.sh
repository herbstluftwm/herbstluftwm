#!/usr/bin/env bash

# print layout of all tags, and colorizes all window ids
# it's useful to get a overview over the list of all windows

hc() { "${herbstclient_command[@]:-herbstclient}" "$@" ;}

hc complete 1 use |
while read tag ; do
    echo -n "$tag "
    indent=$(echo -n "$tag " | sed 's/./ /g')
    # prepend indent, except in first line
    hc layout "$tag" \
        | sed -e "2,\$ s/^/$indent/" \
              -e "s/0x[0-9a-f]\+/$(tput setaf 3)&$(tput sgr0)/g"
done

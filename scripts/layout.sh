#!/bin/bash

# print layout of all tags, and colorizes all window ids
# it's useful to get a overview over the list of all windows

hc=${herbstclient_command:-herbstclient}

$hc complete 1 use |
while read tag ; do
    echo -n "$tag "
    indent=$(echo -n "$tag "|sed 's/./ /g')
    # prepend indent, except in first line
    $hc layout "$tag" \
        | sed "2,\$ s/^/$indent/" \
        | sed "s/\(0x[0-9a-f]\{1,\}\)/$(tput setaf 3)\1$(tput sgr0)/g"
done



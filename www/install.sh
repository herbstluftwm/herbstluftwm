#!/bin/bash

# USAGE:
#   $0 [TARGET]

# target directory, you propably have to parse the right directory to it ;)
target=${1:-cip:.www/herbstluftwm}

files=(
    index.html
    main.css
    herbstclient.html
    herbstluftwm.html
)

rsync -v "${files[@]}" "$target"






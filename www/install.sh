#!/bin/bash


# target directory, you propably have to parse the right directory to it ;)
target=${1:-cip:.www/herbstluftwm}

files=(
    index.html
    main.css
    ../doc/herbstclient.html
    ../doc/herbstluftwm.html
)

rsync -v "${files[@]}" "$target"






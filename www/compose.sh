#!/bin/bash

# usage: $0 A
#   composes a content file A by adding a header and a footer

file="$1"
id="${file%-content.html}"

prefix="herbstluftwm - "

# titles that are shown in <title> and in <h1>
declare -A id2title
id2title=(
    ["index"]="herbstluftwm"
    ["news"]="${prefix}NEWS"
    ["faq"]="${prefix}FAQ"
    ["herbstluftwm"]="herbstluftwm(1)"
    ["herbstclient"]="herbstclient(1)"
)

# how names are shown in the navigation bar
declare -A id2name
id2name=(
    ["index"]="Main"
    ["news"]="NEWS"
    ["faq"]="FAQ"
    ["herbstluftwm"]="herbstluftwm(1)"
    ["herbstclient"]="herbstclient(1)"
)

navigationbar=( index news herbstluftwm herbstclient faq )

title=${id2title[$id]}

#====~===~=========~==
# Header
#====~===~=========~==
cat <<EOF
<html>
 <head>
  <link rel="stylesheet" href="main.css" type="text/css" />
  <title>$title</title>
 </head>
 <body>
  <div id="header">
    <h1>$title</h1>
  </div>
  <div id="content">
EOF

#====~===~=========~==
# Navigation bar
#====~===~=========~==

cat <<EOF
    <div id="#navigationbar">
EOF
for i in "${navigationbar[@]}" ; do
    name="${id2name[$i]:-$i}"
    [ "$id" = "$i" ] && current='class="current" ' || current=''
    cat <<EOF
     <a ${current}href="$i.html">$name</a>
EOF
done
cat <<EOF
    </div>
EOF

#====~===~=========~==
# Content
#====~===~=========~==
cat "$file"

#====~===~=========~==
# Footer
#====~===~=========~==
cat <<EOF
   <div class="footer">
     Generated on $(date +'%Y-%m-%d at %H:%M:%S %Z').
   </div>
  </div>
 </body>
</html>
EOF


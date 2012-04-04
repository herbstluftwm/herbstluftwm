#!/bin/bash

# usage: $0 A
#   composes a content file A by adding a header and a footer

file="$1"
id="${file%-content.html}"

prefix="herbstluftwm - "

# titles that are shown in <title>
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
    ["index"]="Home"
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
  <meta http-equiv="Content-Type" content="application/xhtml+xml; charset=UTF-8" />
  <title>$title</title>
 </head>
 <body>
  <div id="frame">
   <div id="header">
     <h1>herbstluftwm</h1>
     <div id="subheader">
      a manual tiling window manager for X
     </div>
   </div>
EOF

#====~===~=========~==
# Navigation bar
#====~===~=========~==

cat <<EOF
    <table width="100%" id="navigationbar" cellspacing="0">
    <tr>
EOF
for i in "${navigationbar[@]}" ; do
    name="${id2name[$i]:-$i}"
    [ "$id" = "$i" ] && current=' class="curtab"' || current=' class="notab"'
    cat <<EOF
     <td class="notab spacing">&nbsp</td>
     <td${current}><a href="$i.html">$name</a></td>
EOF
done
cat <<EOF
     <td class="notab spacing">&nbsp</td>
    </tr>
    </table>
EOF

#====~===~=========~==
# Content
#====~===~=========~==
cat <<EOF
   <div id="content">
EOF
cat "$file" - <<EOF
    <div class="footer">
      Generated on $(date +'%Y-%m-%d at %H:%M:%S %Z').
    </div>
EOF

#====~===~=========~==
# Footer
#====~===~=========~==
cat <<EOF
   </div>
  </div>
 </body>
</html>
EOF


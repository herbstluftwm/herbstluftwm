#!/usr/bin/env bash

# if you know a cleaner version then grepping everything out of html
# then you are very welcome to improve this script!

echo '<h2>Frequently asked questions</h2>'
echo '<ul class="toc">'
echo '<h4>Table of contents</h4>'
grep '<h2 id=' "$1" \
    | sed 's,<h2 id="\(.*\)">\(.*\)</h2>,<li><a href="#\1">\2</a></li>,'
echo '</ul>'


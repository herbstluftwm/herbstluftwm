#! /usr/bin/env python2

import sys
import datetime
import types
from collections import OrderedDict

tabs = OrderedDict([
    ("Overview", OrderedDict([
        ("index","..."),
    ])),
    ("Doc", OrderedDict([
        ("news", "NEWS"),
        ("herbstluftwm", "herbstluftwm(1)"),
        ("herbstclient", "herbstclient(1)"),
    ])),
    ("FAQ", OrderedDict([
        ("faq", "FAQ"),
    ])),
    ("Wiki", "http://wiki.herbstluftwm.org")
])

page2tab = {}

for title, subpages in tabs.iteritems():
    if not isinstance(subpages, basestring):
        for filename, _ in subpages.iteritems():
            page2tab[filename] = title

windowtitle = "herbstluftwm"

filename = sys.argv[1]
name = filename.replace('-content.html', '')
curtab = page2tab[name]

#====~===~=========~==
# Header
#====~===~=========~==
print """\
<html>
 <head>
  <link rel="stylesheet" href="main.css" type="text/css" />
  <meta http-equiv="Content-Type" content="application/xhtml+xml; charset=UTF-8" />
  <title>{title}</title>
 </head>
 <body>
  <div id="frame">
   <div id="header">
    <div id="logoname">
     <img id="icon" src="herbstluftwm.svg"/>
     <div id="squeezeheader">
       <h1>herbstluftwm</h1>
       <div id="subheader">
        a manual tiling window manager for X
       </div>
     </div>
    </div>
   </div>""".format(title="test")

#====~===~=========~==
# Navigation bar
#====~===~=========~==

print """\
    <table width="100%" id="navigationbar" cellspacing="0">
    <tr>"""

for title, subpages in tabs.iteritems():
    classstring = "notab"
    if title == curtab:
        classstring = "curtab"
    print '<td class="notab spacing">&nbsp</td>'
    if isinstance(subpages, basestring):
        trg = subpages
    else:
        trg = subpages.keys()[0] + ".html"
    print '<td class="{cls}"><a href="{target}">{title}</a></td>'.format(
        cls = classstring,
        target = trg,
        title = title)


print """\
     <td class="notab spacing">&nbsp</td>
    </tr>
    </table>\
"""

subpages = tabs[page2tab[name]]

if len(subpages) > 1:
    print '<div class="subpagebar">'
    for basename, title in subpages.iteritems():
        if basename == name:
            cls = "subpagecur subpage"
        else:
            cls = "subpage"
        print '<span class="{cls}">'.format(cls = cls)
        print '<a href="{url}">{title}</a>'.format(
            url = basename + ".html",title = title)
        print '</span>'
    print "</div>"

print """\
    <div id="content">\
"""

print open(filename).read()


print """\
    <div class="footer">
      Generated on {date}
    </div>
""".format(date=datetime.datetime.now().strftime('%Y-%m-%d at %H:%M:%S %Z'))

#====~===~=========~==
# Footer
#====~===~=========~==
print """\
   </div>
  </div>
 </body>
</html>
"""

# vim: noet

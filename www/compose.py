#!/usr/bin/env python3

import sys
import datetime
from collections import OrderedDict

tabs = OrderedDict([
    ("Overview", OrderedDict([
        ("index", ""),
    ])),
    ("Documentation", OrderedDict([
        ("news", "News"),
        ("migration", "Migration"),
        ("tutorial", "Tutorial"),
        ("object-doc", "Objects"),
        ("herbstluftwm", "herbstluftwm(1)"),
        ("herbstclient", "herbstclient(1)"),
    ])),
    ("FAQ", OrderedDict([
        ("faq", "FAQ"),
    ])),
    ("Download", OrderedDict([
        ("download", "Download"),
    ])),
    ("Users contributions", OrderedDict([
		("contrib", "Users contributions"),
    ])),
    # ("Wiki", "http://wiki.herbstluftwm.org"),
])

page2tab = {
    'imprint': "Imprint and Privacy Policy",
}

filename = sys.argv[1]
name = filename.replace('-content.html', '')
toc = filename.replace('-content.html', '-toc.html')

windowtitle = "herbstluftwm"
for title, subpages in tabs.items():
    if not isinstance(subpages, str):
        for fn, subtitle in subpages.items():
            page2tab[fn] = title
            if not ("" == subtitle) and (name == fn):
                windowtitle = subtitle + " - herbstluftwm"


curtab = page2tab[name]

# ====~===~=========~==
# Header
# ====~===~=========~==
print("""\
<html>
 <head>
  <link rel="stylesheet" href="main.css" type="text/css" />
  <link rel="icon" type="image/x-icon" href="favicon.ico" />
  <meta http-equiv="Content-Type" content="application/xhtml+xml; charset=UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title}</title>
 </head>
 <body>
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
   </div>
  """.format(title=windowtitle))

# ====~===~=========~==
# Navigation bar
# ====~===~=========~==

print("""\
    <ul id="navigationbar">
     <div class="pagewidth">""")

for title, subpages in tabs.items():
    classstring = "notab"
    if title == curtab:
        classstring = "curtab"
    if isinstance(subpages, str):
        trg = subpages
    else:
        trg = list(subpages.keys())[0] + ".html"
    print('<li class="{cls}"><a href="{target}">{title}</a></li>'.format(
        cls=classstring,
        target=trg,
        title=title))


print("""\
     </div>\
    </ul>\
    <div class="tabbarseparator"></div>
""")

subpages = tabs.get(page2tab[name], OrderedDict([]))

if len(subpages) > 1:
    print('<div class="subpagebar">')
    print(' <div class="pagewidth">')
    for basename, title in subpages.items():
        if basename == name:
            cls = "subpagecur subpage"
        else:
            cls = "subpage"
        print('<span class="{cls}">'.format(cls=cls))
        print('<a href="{url}">{title}</a></span>'.format(
            url=basename + ".html", title=title))
    print(" </div>")
    print("</div>")

print("""\
   <div class="pagewidth">\
    <div id="content">\
""")

# possibly table of contents:
try:
    print(open(toc).read())
except IOError:
    # no toc file
    print("<!-- no toc file present -->")
print(open(filename).read())


print("""\
    <div class="footer">
      Generated on {date}
     - <a href=\"imprint.html\">Imprint and Privacy Policy</a>
    </div>
""".format(date=datetime.datetime.now().strftime('%Y-%m-%d at %H:%M:%S %Z')))

# ====~===~=========~==
# Footer
# ====~===~=========~==
print("""\
   </div>
  </div>
 </body>
</html>
""")

# vim: noet

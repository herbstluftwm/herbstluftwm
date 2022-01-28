#!/usr/bin/env python3

"""
Act like 'xmllint', but instead patch the given man page
in docbook xml format using literal2emph.xsl
"""

import sys
import subprocess
import os

# the filename of the docbook xml is probably the first
# command line argument that is not a flag
filename = None
for arg in sys.argv[1:]:
    if len(arg) > 0 and arg[0] != '-':
        filename = arg
        break

xsltproc = 'xsltproc'

doc_directory = os.path.dirname(__file__)

cmd = [
    xsltproc,
    '--output',
    filename,
    os.path.join(doc_directory, 'literal2emph.xsl'),
    filename
]

print("Running: {}".format(str(cmd)), file=sys.stderr)
subprocess.call(cmd, stdin=subprocess.DEVNULL)

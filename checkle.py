#!/usr/bin/env python

import os
import sys
import subprocess

files = subprocess.Popen(['hg', 'log', '-r', 'tip', '--template', '{files}'],
                         stdout=subprocess.PIPE).communicate()[0].split()

status = 0

for name in files:
    if not os.path.exists(name) or name.startswith('medit/data') or \
       name.endswith('.icns') or name.endswith('.png'):
            continue
    f = open(name, 'rb')
    if '\r' in f.read():
        print >> sys.stderr, "%s contains \\r character" % (name,)
        status = 1
    f.close()

sys.exit(status)

#! /usr/bin/env python

import os
import optparse

op = optparse.OptionParser()
op.add_option("-o", "--output", action="store")
op.add_option("-i", "--input", action="store")
(opts, args) = op.parse_args()

assert len(args) == 0
assert opts.input and opts.output

with open(opts.input) as inp:
    with open(opts.output + '.tmp', 'w') as out:
        out.write(inp.read().replace('\014', ''))
if os.path.exists(opts.output):
    os.remove(opts.output)
os.rename(opts.output + '.tmp', opts.output)

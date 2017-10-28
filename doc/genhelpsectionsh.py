#! /usr/bin/env python

import os
import re
import sys
import optparse

op = optparse.OptionParser()
op.add_option("--toc", action="store")
op.add_option("--srcdir", action="store")
op.add_option("--output", action="store")
(opts, args) = op.parse_args()

srcdir = opts.srcdir or '.'

def resolve_filename(filename):
    if os.path.exists(filename):
        return filename
    fullname = os.path.join(srcdir, filename)
    if os.path.exists(fullname):
        return fullname
    raise RuntimeError('could not find file %s' % filename)

def parse_toc(filename):
    filename = resolve_filename(filename)
    dic = {}
    for line in open(filename):
        m = re.search(r'<tocentry linkend="([\w\d_.-]+)"><\?dbhtml filename="([\w\d_.-]+)"\?>', line)
        if m:
            dic[m.group(1)] = m.group(2)
    return dic

def parse_docbook(filename):
    filename = resolve_filename(filename)
    dic = {}
    for line in open(filename):
        m = re.search(r'id\s*=\s*"([\w\d_.-]+)"\s+moo.helpsection\s*=\s*"([\w\d_.-]+)"', line)
        if m:
            dic[m.group(2)] = m.group(1)
        else:
            m = re.search(r'moo.helpsection\s*=\s*"([\w\d_.-]+)"\s+id\s*=\s*"([\w\d_.-]+)"', line)
            if m:
                dic[m.group(1)] = m.group(2)
    return dic

map_id_to_html = parse_toc(opts.toc)
map_hsection_to_id = {}
for f in args:
    map_hsection_to_id.update(parse_docbook(f))

map_hsection_to_html = {
    'PREFS_ACCELS': 'index.html',
    'DIALOG_REPLACE': 'index.html',
    'DIALOG_FIND': 'index.html',
    'FILE_SELECTOR': 'index.html',
    'DIALOG_FIND_FILE': 'index.html',
    'DIALOG_FIND_IN_FILES': 'index.html',
}

for section in map_hsection_to_id:
    map_hsection_to_html[section] = os.path.basename(map_id_to_html[map_hsection_to_id[section]])

map_hsection_to_html['PREFS_PLUGINS'] = map_hsection_to_html['PREFS_DIALOG']
map_hsection_to_html['PREFS_VIEW'] = map_hsection_to_html['PREFS_DIALOG']

def write(out):
    out.write('#ifndef MOO_HELP_SECTIONS_H\n')
    out.write('#define MOO_HELP_SECTIONS_H\n')
    out.write('\n')
    for section in sorted(map_hsection_to_html.keys()):
        out.write('#define HELP_SECTION_%s "%s"\n' % (section, map_hsection_to_html[section]))
    out.write('\n')
    out.write('#endif /* MOO_HELP_SECTIONS_H */\n')

if opts.output:
    with open(opts.output + '.tmp', 'w') as out:
        write(out)
    if os.path.exists(opts.output):
        os.remove(opts.output)
    os.rename(opts.output + '.tmp', opts.output)
else:
    write(sys.stdout)

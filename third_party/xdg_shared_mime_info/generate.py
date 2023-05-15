#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetches, saves, and parses freedesktop.org.xml.in to generate mime_cache.cc.
"""


import os
import re
import sys
import urllib.request
import xml.etree.ElementTree as ET

# Import jinja2 from //third_party/jinja2
current_dir = os.path.dirname(__file__)
src_dir = os.path.join(current_dir, *([os.pardir] * 2))
sys.path.insert(1, os.path.join(src_dir, 'third_party'))
import jinja2

assert __name__ == '__main__'

# A static extension has form such as '*.txt' where it starts with '*.'
# followed by a static string without any of the 5 chars *?[]!
# which are used by fnmatch for globbing.
static_ext = re.compile(r'^\*\.[^*?\[\]!]+$')
ns = ''
mime_types = {}

# Fetch and save local copy of xml.
filename = 'freedesktop.org.xml.in'
url = ('https://gitlab.freedesktop.org/xdg/shared-mime-info/-/raw/master/data/%s'
       % filename)
xml = urllib.request.urlopen(url).read()
with open(os.path.join(current_dir, filename), 'wb') as f:
  f.write(xml)

# Parse xml.
root = ET.fromstring(xml)
for mtype in root:
  mglobs = mtype.findall(
      '{http://www.freedesktop.org/standards/shared-mime-info}glob')
  if mglobs is None:
    continue
  for mglob in mglobs:
    if not static_ext.match(mglob.attrib['pattern']):
      continue
    ext = mglob.attrib['pattern'][2:]
    if not mglob.attrib.get('case-sensitive', False):
      ext = ext.lower()
    weight = int(mglob.attrib['weight'])
    if ext not in mime_types or weight > mime_types[ext]['weight']:
      mime_types[ext] = {
        'ext': ext,
        'mime': mtype.attrib['type'],
        'weight': weight,
      }

# Update mime_cache.cc.
values = list(mime_types.values())
values.sort(key=lambda x: x['ext'])
env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(current_dir),
    lstrip_blocks=True,
    trim_blocks=True)
template = env.get_template('mime_cache.cc.tmpl')
with open(os.path.join(current_dir, 'mime_cache.gen.cc'), 'w') as f:
  f.write(template.render({'mime_types': values}))


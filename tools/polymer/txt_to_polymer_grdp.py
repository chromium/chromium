#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import with_statement
from __future__ import print_function

import argparse
import os
import string
import sys


FILE_TEMPLATE = \
"""<?xml version="1.0" encoding="utf-8"?>
<!--
  This file is partially generated. See note below about the "partially" part.

  Please use 'src/tools/polymer/polymer_grdp_to_txt.py' and
  'src/tools/polymer/txt_to_polymer_grdp.py' to modify it, if possible.

  'polymer_grdp_to_txt.py' converts 'polymer_resources.grdp' to a plain list of
  used Polymer components:
    ...
    iron-iron-iconset/iron-iconset-extracted.js
    iron-iron-iconset/iron-iconset.html
    ...

  'txt_to_polymer_grdp.py' converts list back to GRDP file.

  Usage:
    $ polymer_grdp_to_txt.py polymer_resources.grdp \-\-polymer_version=%(version)s > /tmp/list.txt
    $ vim /tmp/list.txt
    $ txt_to_polymer_grdp.py /tmp/list.txt \-\-polymer_version=%(version)s > polymer_resources.grdp

  NOTE: Regenerating this file will eliminate all previous <if expr> statements.
  Please restore these manually.
-->
<grit-part>
  <!-- Polymer %(version)s.0 -->
%(contents)s
%(web_animations)s
</grit-part>
"""

DEFINITION_TEMPLATE_WEB_ANIMATIONS = \
"""  <structure name="IDR_POLYMER_1_0_WEB_ANIMATIONS_JS_WEB_ANIMATIONS_NEXT_LITE_MIN_JS"
             file="../../../third_party/web-animations-js/sources/web-animations-next-lite.min.js"
             type="chrome_html"
             compress="gzip" />"""

DEFINITION_TEMPLATE = \
"""  <structure name="%(name)s"
             file="../../../third_party/polymer/v%(version)s_0/components-chromium/%(path)s"
             type="chrome_html"
             compress="gzip" />"""


def PathToGritId(polymer_version, path):
  table = string.maketrans(string.lowercase + '/.-', string.uppercase + '___')
  return ('IDR_POLYMER_%s_0_' % polymer_version) + path.translate(table)


def SortKey(polymer_version, record):
  return (record, PathToGritId(record, polymer_version))


def ParseRecord(record):
  return record.strip()


class FileNotFoundException(Exception):
  pass


_HERE = os.path.dirname(os.path.realpath(__file__))


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('input')
  parser.add_argument('--polymer_version', required=True)
  args = parser.parse_args(argv)

  polymer_version = args.polymer_version

  polymer_dir = os.path.join(_HERE, os.pardir, os.pardir,
      'third_party', 'polymer', 'v%s_0' % polymer_version,
      'components-chromium')

  with open(args.input) as f:
    records = [ParseRecord(r) for r in f if not r.isspace()]
  lines = []
  for path in sorted(set(records), key=lambda r: SortKey(r, polymer_version)):
    full_path = os.path.normpath(os.path.join(polymer_dir, path))
    if not os.path.exists(full_path):
      raise FileNotFoundException('%s not found' % full_path)

    lines.append(DEFINITION_TEMPLATE % {
        'version': polymer_version,
        'name': PathToGritId(polymer_version, path),
        'path': path})

  print(FILE_TEMPLATE % {
      'contents':
          '\n'.join(lines),
      'web_animations':
          '' if polymer_version == '3' else DEFINITION_TEMPLATE_WEB_ANIMATIONS,
      'version':
          polymer_version
  })


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

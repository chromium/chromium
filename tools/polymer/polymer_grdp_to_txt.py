#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import sys
import xml.sax


class PathsExtractor(xml.sax.ContentHandler):

  def __init__(self, polymer_version):
    self.paths = []
    self.polymer_version = polymer_version

  def startElement(self, name, attrs):
    if name != 'structure':
      return
    path = attrs['file']
    if path.startswith('../../../third_party/web-animations-js'):
      return
    prefix = ('../../../third_party/polymer/v%s_0/components-chromium/' %
        self.polymer_version)
    if path.startswith(prefix):
      self.paths.append(path[len(prefix):])
    else:
      raise Exception("Unexpected path %s." % path)

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('input')
  parser.add_argument('--polymer_version', required=True)
  args = parser.parse_args(argv)

  xml_handler = PathsExtractor(args.polymer_version)
  xml.sax.parse(args.input, xml_handler)
  print('\n'.join(sorted(xml_handler.paths)))


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

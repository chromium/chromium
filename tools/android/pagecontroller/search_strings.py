#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Finds R.string.* ids in android_chrome_sstrings.grd file."""

import argparse
import os
import re
import sys
import xml.etree.cElementTree as ElementTree

_SRC_DIR = os.path.abspath(os.path.join(
      os.path.dirname(__file__), '..', '..', '..'))

_DEFAULT_GRD = os.path.join(_SRC_DIR,
    'chrome/android/java/strings/android_chrome_strings.grd')
_MATCH_MSG = u'  R.string.{}:\n    Text = "{}"\n    Desc = "{}"'

class GrdSearch(object):
  """ Search strings.grd file. """
  def __init__(self, grd_path):
    grd_dom = ElementTree.parse(grd_path)
    messages = grd_dom.findall('.//message')
    self.message_lookup = {}
    for message in messages:
      self.message_lookup[message.get('name')] = {
          'text': message.itertext().next().strip(),
          'desc': message.get('desc')}

  def find_term_in_grd(self, term, is_regex):
    """ Returns matches for term in the form (string id, text, desc) """
    results = []
    for name,value in self.message_lookup.iteritems():
      if ((not is_regex and value['text'] == term) or
          (is_regex and re.match(term, value['text']))):
        results.append((name[4:].lower(), value['text'], value['desc']))
    return results

def main():
  parser = argparse.ArgumentParser(description='Find clank string resource ids'
                                   ' based on string content.')
  parser.add_argument('-r', '--regex', action='store_true',
                      help='perform regex search instead of literal')
  parser.add_argument('-g', '--grd-file',
                      default=_DEFAULT_GRD,
                      help='strings.grd file, default: {}'.format(_DEFAULT_GRD))
  parser.add_argument('terms', nargs='+',
                      help='Search terms.')

  args = parser.parse_args(sys.argv[1:])

  searcher = GrdSearch(args.grd_file)

  for t in args.terms:
    print('{} search term: "{}", R.string.* matches:'.format(
        ('Regex' if args.regex else 'Literal'), t))
    for name, text, desc, in searcher.find_term_in_grd(t.decode('utf-8'),
                                                       args.regex):
      print(_MATCH_MSG.format(name, text, desc))
    print('')

if __name__ == '__main__':
  sys.exit(main())

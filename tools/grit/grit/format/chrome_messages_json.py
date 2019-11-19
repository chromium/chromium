# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Formats as a .json file that can be used to localize Google Chrome
extensions."""

from __future__ import print_function

from json import JSONEncoder

from grit import constants
from grit.node import message

def Format(root, lang='en', output_dir='.'):
  """Format the messages as JSON."""
  yield '{'

  encoder = JSONEncoder(ensure_ascii=False)
  format = '"%s":{"message":%s%s}'
  placeholder_format = '"%i":{"content":"$%i"}'
  first = True
  for child in root.ActiveDescendants():
    if isinstance(child, message.MessageNode):
      id = child.attrs['name']
      if id.startswith('IDR_') or id.startswith('IDS_'):
        id = id[4:]

      translation_missing = child.GetCliques()[0].clique.get(lang) is None;
      if (child.ShouldFallbackToEnglish() and translation_missing and
          lang != constants.FAKE_BIDI):
          # Skip the string if it's not translated. Chrome will fallback
          # to English automatically.
          continue

      loc_message = encoder.encode(child.ws_at_start + child.Translate(lang) +
                                   child.ws_at_end)

      # Replace $n place-holders with $n$ and add an appropriate "placeholders"
      # entry. Note that chrome.i18n.getMessage only supports 9 placeholders:
      # https://developer.chrome.com/extensions/i18n#method-getMessage
      placeholders = ''
      for i in range(1, 10):
        if loc_message.find('$%d' % i) == -1:
          break
        loc_message = loc_message.replace('$%d' % i, '$%d$' % i)
        if placeholders:
          placeholders += ','
        placeholders += placeholder_format % (i, i)

      if not first:
        yield ','
      first = False

      if placeholders:
        placeholders = ',"placeholders":{%s}' % placeholders
      yield format % (id, loc_message, placeholders)

  yield '}'

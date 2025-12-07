# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Formats as a .json file that can be used to localize Google Chrome
extensions."""


from json import JSONEncoder

from grit import constants
from grit.node import message


def Format(root, lang='en', gender=None, output_dir='.'):
  """Format the messages as JSON."""

  assert gender is None, "chrome_message_json doesn't support gender " \
      f"translations, yet Format() was called with gender {gender}"

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

      translation_missing = not child.GetCliques()[0].HasTranslation(
          lang, constants.DEFAULT_GENDER)
      if (child.ShouldFallbackToEnglish() and translation_missing
          and lang not in constants.PSEUDOLOCALES):
        # Skip the string if it's not translated. Chrome will fallback
        # to English automatically.
        continue

      loc_message = encoder.encode(
          child.ws_at_start + child.Translate(lang, constants.DEFAULT_GENDER) +
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

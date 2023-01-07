# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Regular expression helpers."""

import re


def _CreateIdentifierRegex(parts):
  assert parts
  if parts[0]:
    # Allow word boundary or a _ prefix.
    prefix_pattern = r'\b_?'
  else:
    # Allow word boundary, _, or a case transition.
    prefix_pattern = r'(?:\b|(?<=_)|(?<=[a-z])(?=[A-Z]))'
    parts = parts[1:]
    assert parts

  if parts[-1]:
    # Allow word boundary, trailing _, or single trailing digit.
    suffix_pattern = r'\d?_?\b'
  else:
    # Allow word boundary, _, or a case transition.
    suffix_pattern = r'(?:\b|(?=_|\d)|(?<=[a-z])(?=[A-Z]))'
    parts = parts[:-1]
    assert parts

  shouty_pattern = '_'.join(a.upper() for a in parts)
  snake_pattern = '_'.join(a.lower() for a in parts)
  camel_remainder = parts[0][1:]
  for token in parts[1:]:
    camel_remainder += token.title()
  first_letter = parts[0][0]
  prefixed_pattern = '[a-z]' + first_letter.upper() + camel_remainder
  camel_pattern = '[%s%s]%s' % (first_letter.lower(), first_letter.upper(),
                                camel_remainder)
  middle_patterns = '|'.join(
      (shouty_pattern, snake_pattern, prefixed_pattern, camel_pattern))
  return r'(?:%s(?:%s)%s)' % (prefix_pattern, middle_patterns, suffix_pattern)


def ExpandRegexIdentifierPlaceholder(pattern):
  """Expands {{identifier}} within a given regular expression.

  Returns |pattern|, with the addition that {{some_name}} is expanded to match:
      SomeName, kSomeName, SOME_NAME, etc.

  To match part of a name, use {{_some_name_}}. This will additionally match:
       kPrefixSomeNameSuffix, PREFIX_SOME_NAME_SUFFIX, etc.

  Note: SymbolGroup.Where* methods call this function already, so there is
  generally no need to call it directly.
  """
  return re.sub(r'\{\{(.*?)\}\}',
                lambda m: _CreateIdentifierRegex(m.group(1).split('_')),
                pattern)

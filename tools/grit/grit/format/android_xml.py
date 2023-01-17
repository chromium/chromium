# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Produces localized strings.xml files for Android.

In cases where an "android" type output file is requested in a grd, the classes
in android_xml will process the messages and translations to produce a valid
strings.xml that is properly localized with the specified language.

For example if the following output tag were to be included in a grd file
  <outputs>
    ...
    <output filename="values-es/strings.xml" type="android" lang="es" />
    ...
  </outputs>

for a grd file with the following messages:

  <message name="IDS_HELLO" desc="Simple greeting">Hello</message>
  <message name="IDS_WORLD" desc="The world">world</message>

and there existed an appropriate xtb file containing the Spanish translations,
then the output would be:

  <?xml version="1.0" encoding="utf-8"?>
  <resources xmlns:android="http://schemas.android.com/apk/res/android">
    <string name="hello">"Hola"</string>
    <string name="world">"mundo"</string>
  </resources>

which would be written to values-es/strings.xml and usable by the Android
resource framework.

Advanced usage
--------------

To process only certain messages in a grd file, tag each desired message by
adding "android_java" to formatter_data. Then set the environmental variable
ANDROID_JAVA_TAGGED_ONLY to "true" when building the grd file. For example:

  <message name="IDS_HELLO" formatter_data="android_java">Hello</message>

To generate Android plurals (aka "quantity strings"), use the ICU plural syntax
in the grd file. This will automatically be transformed into a <purals> element
in the output xml file. For example:

  <message name="IDS_CATS">
    {NUM_CATS, plural,
    =1 {1 cat}
    other {# cats}}
  </message>

  will produce

  <plurals name="cats">
    <item quantity="one">1 Katze</item>
    <item quantity="other">%d Katzen</item>
  </plurals>
"""


import os
import re
import xml.sax.saxutils

from grit import lazy_re
from grit.node import message


# When this environmental variable has value "true", only tagged messages will
# be outputted.
_TAGGED_ONLY_ENV_VAR = 'ANDROID_JAVA_TAGGED_ONLY'
_TAGGED_ONLY_DEFAULT = False

# In tagged-only mode, only messages with this tag will be ouputted.
_EMIT_TAG = 'android_java'

_NAME_PATTERN = lazy_re.compile(r'IDS_(?P<name>[A-Z0-9_]+)\Z')

# Most strings are output as a <string> element. Note the double quotes
# around the value to preserve whitespace.
_STRING_TEMPLATE = '<string name="%s">"%s"</string>\n'

# Some strings are output as a <plurals> element.
_PLURALS_TEMPLATE = '<plurals name="%s">\n%s</plurals>\n'
_PLURALS_ITEM_TEMPLATE = '  <item quantity="%s">%s</item>\n'

# Matches e.g. "{HELLO, plural, HOW ARE YOU DOING}", while capturing
# "HOW ARE YOU DOING" in <items>. The en-XA pseudolocale adds a set of words
# beginning with " - one" after the plural block which is also captured.
_PLURALS_PATTERN = lazy_re.compile(
    r'\{[A-Z_]+,\s*plural,(?P<items>.*)\}(?P<pseudolong> - one.*)?$', flags=re.S)

# Repeatedly matched against the <items> capture in _PLURALS_PATTERN,
# to match "<quantity>{<value>}".
_PLURALS_ITEM_PATTERN = lazy_re.compile(r'(?P<quantity>\S+?)\s*'
                                        r'\{(?P<value>.*?)\}')
_PLURALS_QUANTITY_MAP = {
  '=0': 'zero',
  'zero': 'zero',
  '=1': 'one',
  'one': 'one',
  '=2': 'two',
  'two': 'two',
  'few': 'few',
  'many': 'many',
  'other': 'other',
}


def Format(root, lang='en', output_dir='.'):
  yield ('<?xml version="1.0" encoding="utf-8"?>\n'
          '<resources '
          'xmlns:android="http://schemas.android.com/apk/res/android">\n')

  tagged_only = _TAGGED_ONLY_DEFAULT
  if _TAGGED_ONLY_ENV_VAR in os.environ:
    tagged_only = os.environ[_TAGGED_ONLY_ENV_VAR].lower()
    if tagged_only == 'true':
      tagged_only = True
    elif tagged_only == 'false':
      tagged_only = False
    else:
      raise Exception('env variable ANDROID_JAVA_TAGGED_ONLY must have value '
                      'true or false. Invalid value: %s' % tagged_only)

  for item in root.ActiveDescendants():
    with item:
      if ShouldOutputNode(item, tagged_only):
        yield _FormatMessage(item, lang)

  yield '</resources>\n'


def ShouldOutputNode(node, tagged_only):
  """Returns true if node should be outputted.

  Args:
      node: a Node from the grd dom
      tagged_only: true, if only tagged messages should be outputted
  """
  return (isinstance(node, message.MessageNode) and
          (not tagged_only or _EMIT_TAG in node.formatter_data))


def _FormatPluralMessage(message):
  """Compiles ICU plural syntax to the body of an Android <plurals> element.

  1. In a .grd file, we can write a plural string like this:

    <message name="IDS_THINGS">
      {NUM_THINGS, plural,
      =1 {1 thing}
      other {# things}}
    </message>

  2. The Android equivalent looks like this:

    <plurals name="things">
      <item quantity="one">1 thing</item>
      <item quantity="other">%d things</item>
    </plurals>

  This method takes the body of (1) and converts it to the body of (2).

  If the message is *not* a plural string, this function returns `None`.
  If the message includes quantities without an equivalent format in Android,
  it raises an exception.
  """
  ret = {}
  plural_match = _PLURALS_PATTERN.match(message)
  if not plural_match:
    return None
  body_in = plural_match.group('items').strip()
  # If this is the en-XA pseudolocale get the extra words added.
  psudolong_extra = plural_match.group('pseudolong')
  if not psudolong_extra:
    psudolong_extra = ''
  lines = []
  quantities_so_far = set()
  for item_match in _PLURALS_ITEM_PATTERN.finditer(body_in):
    quantity_in = item_match.group('quantity')
    quantity_out = _PLURALS_QUANTITY_MAP.get(quantity_in)
    value_in = item_match.group('value') + psudolong_extra
    value_out = '"' + value_in.replace('#', '%d') + '"'
    if quantity_out:
      # only one line per quantity out (https://crbug.com/787488)
      if quantity_out not in quantities_so_far:
        quantities_so_far.add(quantity_out)
        lines.append(_PLURALS_ITEM_TEMPLATE % (quantity_out, value_out))
    else:
      raise Exception('Unsupported plural quantity for android '
                      'strings.xml: %s' % quantity_in)
  return ''.join(lines)


def _FormatMessage(item, lang):
  """Writes out a single string as a <resource/> element."""

  mangled_name = item.GetTextualIds()[0]
  match = _NAME_PATTERN.match(mangled_name)
  if not match:
    raise Exception('Unexpected resource name: %s' % mangled_name)
  name = match.group('name').lower()

  value = item.ws_at_start + item.Translate(lang) + item.ws_at_end
  # Replace < > & with &lt; &gt; &amp; to ensure we generate valid XML and
  # replace ' " with \' \" to conform to Android's string formatting rules.
  value = xml.sax.saxutils.escape(value, {"'": "\\'", '"': '\\"'})

  plurals = _FormatPluralMessage(value)
  if plurals:
    return _PLURALS_TEMPLATE % (name, plurals)
  else:
    return _STRING_TEMPLATE % (name, value)

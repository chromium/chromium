# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Holds the constants for pretty printing histograms.xml."""

import os
import re
import sys

# Import the metrics/common module for pretty print xml.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import pretty_print_xml

# Desired order for tag and tag attributes. The *_ATTRIBUTE_ORDER maps are also
# used to determine the validity of tag names.
# { tag_name: [attribute_name, ...] }
ATTRIBUTE_ORDER = {
    'affected-histogram': ['name'],
    'component': [],
    'details': [],
    'enum': ['name'],
    'enums': [],
    'histogram': ['base', 'name', 'enum', 'units', 'expires_after'],
    'histogram-configuration': ['logsource'],
    'histogram_suffixes': ['name', 'separator', 'ordering'],
    'histogram_suffixes_list': [],
    'histograms': [],
    'int': ['value', 'label'],
    'obsolete': [],
    'owner': [],
    'suffix': ['base', 'name', 'label'],
    'summary': [],
    'with-suffix': ['name'],
}

# Attribute names that must be explicitly specified on nodes that support them.
REQUIRED_ATTRIBUTES = [
    # TODO(isherman): Make the 'label' attribute required as well. This requires
    # fixing up existing suffixes that omit a label.
    'name',
    'separator',
    'value',
]

# Tag names for top-level nodes whose children we don't want to indent.
TAGS_THAT_DONT_INDENT = [
    'histogram-configuration',
    'histograms',
    'histogram_suffixes_list',
    'enums',
]

# Extra vertical spacing rules for special tag names.
# {tag_name: (newlines_after_open, newlines_before_close, newlines_after_close)}
TAGS_THAT_HAVE_EXTRA_NEWLINE = {
    'histogram-configuration': (2, 1, 1),
    'histograms': (2, 1, 1),
    'histogram_suffixes_list': (2, 1, 1),
    'histogram_suffixes': (1, 1, 1),
    'enums': (2, 1, 1),
    'histogram': (1, 1, 1),
    'enum': (1, 1, 1),
}

# Tags that we allow to be squished into a single line for brevity.
TAGS_THAT_ALLOW_SINGLE_LINE = ['summary', 'int', 'owner', 'component']

LOWERCASE_NAME_FN = lambda n: n.get('name').lower()


def _NaturalSortByName(node):
  """Sort by name, ordering numbers in the way humans expect."""
  # See: https://blog.codinghorror.com/sorting-for-humans-natural-sort-order/
  name = node.get('name').lower()
  convert = lambda text: int(text) if text.isdigit() else text
  return [convert(c) for c in re.split('([0-9]+)', name)]


# Tags whose children we want to alphabetize. The key is the parent tag name,
# and the value is a list of pairs of tag name and key functions that maps each
# child node to the desired sort key.
TAGS_ALPHABETIZATION_RULES = {
    'histograms': [('histogram', LOWERCASE_NAME_FN)],
    'enums': [('enum', LOWERCASE_NAME_FN)],
    'enum': [('int', lambda n: int(n.get('value')))],
    'histogram_suffixes_list': [('histogram_suffixes', LOWERCASE_NAME_FN)],
    'histogram_suffixes': [
        ('obsolete', lambda n: None),
        ('suffix', _NaturalSortByName),
        ('affected-histogram', LOWERCASE_NAME_FN),
    ],
}


def GetPrintStyle():
  """Returns an XmlStyle object for pretty printing histograms."""
  return pretty_print_xml.XmlStyle(ATTRIBUTE_ORDER,
                                   REQUIRED_ATTRIBUTES,
                                   TAGS_THAT_HAVE_EXTRA_NEWLINE,
                                   TAGS_THAT_DONT_INDENT,
                                   TAGS_THAT_ALLOW_SINGLE_LINE,
                                   TAGS_ALPHABETIZATION_RULES)

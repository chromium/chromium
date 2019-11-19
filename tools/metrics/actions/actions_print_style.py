# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Holds the constants for pretty printing actions.xml."""

import os
import sys

# Import the metrics/common module for pretty print xml.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import pretty_print_xml

# Desired order for tag and tag attributes. The *_ATTRIBUTE_ORDER maps are also
# used to determine the validity of tag names.
# { tag_name: [attribute_name, ...] }
ATTRIBUTE_ORDER = {
    'action': ['name', 'not_user_triggered'],
    'action-suffix': ['separator', 'ordering'],
    'actions': [],
    'actions-suffixes': [],
    'affected-action': ['name'],
    'description': [],
    'obsolete': [],
    'owner': [],
    'suffix': ['name', 'label'],
    'with-suffix': ['name'],
}

# Attribute names that must be explicitly specified on nodes that support them.
REQUIRED_ATTRIBUTES = [
    'label',
    'name',
    'separator',
]

# Tag names for top-level nodes whose children we don't want to indent.
TAGS_THAT_DONT_INDENT = [
    'actions',
]

# Extra vertical spacing rules for special tag names.
# {tag_name: (newlines_after_open, newlines_before_close, newlines_after_close)}
TAGS_THAT_HAVE_EXTRA_NEWLINE = {
    'actions': (2, 1, 1),
    'action': (1, 1, 1),
    'action-suffix': (1, 1, 1),
}

# Tags that we allow to be squished into a single line for brevity.
TAGS_THAT_ALLOW_SINGLE_LINE = ['obsolete', 'owner', 'description']

LOWERCASE_NAME_FN = lambda n: n.get('name').lower()

# Tags whose children we want to alphabetize. The key is the parent tag name,
# and the value is a list of pairs of tag name and key functions that maps each
# child node to the desired sort key.
TAGS_ALPHABETIZATION_RULES = {
    'actions': [
        ('action', LOWERCASE_NAME_FN),
        ('action-suffix', lambda n: None),
    ],
    'action-suffix': [
        ('suffix', LOWERCASE_NAME_FN),
        ('affected-action', LOWERCASE_NAME_FN),
    ],
}


def GetPrintStyle():
  """Returns an XmlStyle object for pretty printing actions."""
  return pretty_print_xml.XmlStyle(ATTRIBUTE_ORDER,
                                   REQUIRED_ATTRIBUTES,
                                   TAGS_THAT_HAVE_EXTRA_NEWLINE,
                                   TAGS_THAT_DONT_INDENT,
                                   TAGS_THAT_ALLOW_SINGLE_LINE,
                                   TAGS_ALPHABETIZATION_RULES)

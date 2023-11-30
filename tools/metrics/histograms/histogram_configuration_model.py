# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Model objects for histograms.xml contents."""

import os
import sys
import re

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models

_OBSOLETE_TYPE = models.TextNodeType('obsolete')
_OWNER_TYPE = models.TextNodeType('owner', single_line=True)
# If present, it's intentional that the histogram is currently expired and
# automation should not suggest for its implementation to be cleaned up.
_EXPIRED_INTENTIONALLY_TYPE = models.TextNodeType('expired_intentionally')
_COMPONENT_TYPE = models.TextNodeType('component', single_line=True)
_SUMMARY_TYPE = models.TextNodeType('summary', single_line=True)

# A key for sorting XML nodes by the lower case of the value of |attribute|.
_LOWERCASE_FN = lambda attribute: (lambda node: node.get(attribute).lower())

# A key for sorting XML nodes by the value of |attribute|, cast as integer.
_INTEGER_FN = lambda attribute: (lambda node: int(node.get(attribute)))

# A constant function as the sorting key for nodes whose orderings should be
# kept as given in the XML file within their parent node.
_KEEP_ORDER = lambda node: 1


# A function for natural-sorting XML nodes, used for sorting <suffix> by their
# name attribute in a way that humans understand.
# i.e. "suffix11" should come after "suffix2"
def _NaturalSortByName(node):
  """Sort by name, ordering numbers in the way humans expect."""
  # See: https://blog.codinghorror.com/sorting-for-humans-natural-sort-order/
  name = node.get('name').lower()
  convert = lambda text: int(text) if text.isdigit() else text
  return [convert(c) for c in re.split('([0-9]+)', name)]

# The following types are used for enums.xml.
_INT_TYPE = models.ObjectNodeType(
    'int',
    attributes=[
        ('value', str, r'^[-1]|[0-9]+$'),
        ('label', str, None),
    ],
    required_attributes=['value'],
    text_attribute=True,
    single_line=True,
)

_ENUM_TYPE = models.ObjectNodeType(
    'enum',
    attributes=[
        ('name', str, r'^[A-Za-z0-9_.]+$'),
    ],
    required_attributes=['name'],
    alphabetization=[
        (_OBSOLETE_TYPE.tag, _KEEP_ORDER),
        (_SUMMARY_TYPE.tag, _KEEP_ORDER),
        (_INT_TYPE.tag, _INTEGER_FN('value')),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_OBSOLETE_TYPE.tag, _OBSOLETE_TYPE, multiple=False),
        models.ChildType(_SUMMARY_TYPE.tag, _SUMMARY_TYPE, multiple=False),
        models.ChildType(_INT_TYPE.tag, _INT_TYPE, multiple=True),
    ])

_ENUMS_TYPE = models.ObjectNodeType(
    'enums',
    alphabetization=[
        (_ENUM_TYPE.tag, _LOWERCASE_FN('name')),
    ],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_ENUM_TYPE.tag, _ENUM_TYPE, multiple=True),
    ])

# The following types are used for histograms.xml.
IMPROVEMENT_DIRECTION_HIGHER_IS_BETTER = 'HIGHER_IS_BETTER'
IMPROVEMENT_DIRECTION_LOWER_IS_BETTER = 'LOWER_IS_BETTER'
IMPROVEMENT_DIRECTION_NEITHER_IS_BETTER = 'NEITHER_IS_BETTER'

IMPROVEMENT_DIRECTION_VALID_VALUES = (
    IMPROVEMENT_DIRECTION_HIGHER_IS_BETTER,
    IMPROVEMENT_DIRECTION_LOWER_IS_BETTER,
    IMPROVEMENT_DIRECTION_NEITHER_IS_BETTER,
)

_IMPROVEMENT_TYPE = models.ObjectNodeType(
    'improvement',
    attributes=[
        (
            'direction',
            str,
            r'^(' + '|'.join(IMPROVEMENT_DIRECTION_VALID_VALUES) + ')$',
        ),
    ],
    required_attributes=['direction'],
    text_attribute=False,
    single_line=True,
)

_VARIANT_TYPE = models.ObjectNodeType(
    'variant',
    attributes=[
        ('name', str, None),
        ('summary', str, None),
    ],
    required_attributes=['name'],
    alphabetization=[
        (_OWNER_TYPE.tag, _KEEP_ORDER),
    ],
    children=[
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
    ],
)

_VARIANTS_TYPE = models.ObjectNodeType(
    'variants',
    attributes=[
        ('name', str, None),
    ],
    required_attributes=['name'],
    alphabetization=[
        (_VARIANT_TYPE.tag, _NaturalSortByName)
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_VARIANT_TYPE.tag, _VARIANT_TYPE, multiple=True),
    ])

_TOKEN_TYPE = models.ObjectNodeType(
    'token',
    attributes=[
        ('key', str, None),
        ('variants', str, None)
    ],
    required_attributes=['key'],
    alphabetization=[
        (_VARIANT_TYPE.tag, _NaturalSortByName)
    ],
    children=[
        models.ChildType(_VARIANT_TYPE.tag, _VARIANT_TYPE, multiple=True),
    ])

_EXPIRED_AFTER_RE = (
    r'^$|^\d{4}\-(0?[1-9]|1[012])\-(0?[1-9]|[12][0-9]|3[01])$|^M[0-9]+$|^never$'
)

_HISTOGRAM_TYPE = models.ObjectNodeType(
    'histogram',
    attributes=[
        ('base', str, r'^$|^true|false$'),
        ('name', str, None),
        ('enum', str, r'^[A-Za-z0-9._]*$'),
        ('units', str, None),
        ('expires_after', str, _EXPIRED_AFTER_RE),
    ],
    required_attributes=['name'],
    alphabetization=[
        (_EXPIRED_INTENTIONALLY_TYPE.tag, _KEEP_ORDER),
        (_OWNER_TYPE.tag, _KEEP_ORDER),
        (_COMPONENT_TYPE.tag, _KEEP_ORDER),
        (_IMPROVEMENT_TYPE.tag, _KEEP_ORDER),
        (_SUMMARY_TYPE.tag, _KEEP_ORDER),
        (_TOKEN_TYPE.tag, _KEEP_ORDER),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_EXPIRED_INTENTIONALLY_TYPE.tag,
                         _EXPIRED_INTENTIONALLY_TYPE,
                         multiple=False),
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
        models.ChildType(_COMPONENT_TYPE.tag, _COMPONENT_TYPE, multiple=True),
        models.ChildType(_SUMMARY_TYPE.tag, _SUMMARY_TYPE, multiple=False),
        models.ChildType(_TOKEN_TYPE.tag, _TOKEN_TYPE, multiple=True),
        models.ChildType(_IMPROVEMENT_TYPE.tag,
                         _IMPROVEMENT_TYPE,
                         multiple=False),
    ])

_HISTOGRAMS_TYPE = models.ObjectNodeType(
    'histograms',
    alphabetization=[
        (_VARIANTS_TYPE.tag, _LOWERCASE_FN('name')),
        (_HISTOGRAM_TYPE.tag, _LOWERCASE_FN('name')),
    ],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_VARIANTS_TYPE.tag, _VARIANTS_TYPE, multiple=True),
        models.ChildType(_HISTOGRAM_TYPE.tag, _HISTOGRAM_TYPE, multiple=True),
    ])

_SUFFIX_TYPE = models.ObjectNodeType('suffix',
                                     attributes=[
                                         ('base', str, r'^$|^true|false$'),
                                         ('name', str, None),
                                         ('label', str, None),
                                     ],
                                     required_attributes=['name'])

_WITH_SUFFIX_TYPE = models.ObjectNodeType(
    'with-suffix',
    attributes=[
        ('name', str, None),
    ],
    required_attributes=['name'])

_AFFECTED_HISTOGRAM_TYPE = models.ObjectNodeType(
    'affected-histogram',
    attributes=[
        ('name', str, None),
    ],
    required_attributes=['name'],
    children=[
        models.ChildType(_WITH_SUFFIX_TYPE.tag,
                         _WITH_SUFFIX_TYPE, multiple=True),
    ])

_HISTOGRAM_SUFFIXES_TYPE = models.ObjectNodeType(
    'histogram_suffixes',
    attributes=[
        ('name', str, r'^$|^[A-Za-z0-9_.]+$'),
        ('separator', str, r'^$|^[\._]+$'),
        ('ordering', str, r'^$|suffix|^prefix(,[0-9]+)?$'),
    ],
    required_attributes=['name', 'separator'],
    alphabetization=[(_SUFFIX_TYPE.tag, _NaturalSortByName),
                     (_AFFECTED_HISTOGRAM_TYPE.tag, _LOWERCASE_FN('name'))],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
        models.ChildType(_SUFFIX_TYPE.tag, _SUFFIX_TYPE, multiple=True),
        models.ChildType(_AFFECTED_HISTOGRAM_TYPE.tag,
                         _AFFECTED_HISTOGRAM_TYPE,
                         multiple=True),
    ])

_HISTOGRAM_SUFFIXES_LIST_TYPE = models.ObjectNodeType(
    'histogram_suffixes_list',
    alphabetization=[(_HISTOGRAM_SUFFIXES_TYPE.tag, _LOWERCASE_FN('name'))],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_HISTOGRAM_SUFFIXES_TYPE.tag,
                         _HISTOGRAM_SUFFIXES_TYPE,
                         multiple=True),
    ])

_HISTOGRAM_CONFIGURATION_TYPE = models.ObjectNodeType(
    'histogram-configuration',
    alphabetization=[
        (_ENUMS_TYPE.tag, _KEEP_ORDER),
        (_HISTOGRAMS_TYPE.tag, _KEEP_ORDER),
        (_HISTOGRAM_SUFFIXES_LIST_TYPE.tag, _KEEP_ORDER),
    ],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_ENUMS_TYPE.tag, _ENUMS_TYPE, multiple=False),
        models.ChildType(_HISTOGRAMS_TYPE.tag, _HISTOGRAMS_TYPE,
                         multiple=False),
        models.ChildType(_HISTOGRAM_SUFFIXES_LIST_TYPE.tag,
                         _HISTOGRAM_SUFFIXES_LIST_TYPE,
                         multiple=False),
    ])

HISTOGRAM_CONFIGURATION_XML_TYPE = models.DocumentType(
    _HISTOGRAM_CONFIGURATION_TYPE)


def PrettifyTree(input_tree):
  """Parses the tree representation of the XML and return a
  pretty-printed version.

  Args:
    input_tree: A tree representation of the XML, which might take the
    form of an ET tree or minidom doc.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  histograms = HISTOGRAM_CONFIGURATION_XML_TYPE.Parse(input_tree)
  return HISTOGRAM_CONFIGURATION_XML_TYPE.PrettyPrint(histograms)

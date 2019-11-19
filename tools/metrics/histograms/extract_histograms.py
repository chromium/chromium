# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extract histogram names from the description XML file.

For more information on the format of the XML file, which is self-documenting,
see histograms.xml; however, here is a simple example to get you started. The
XML below will generate the following five histograms:

    HistogramTime
    HistogramEnum
    HistogramEnum_Chrome
    HistogramEnum_IE
    HistogramEnum_Firefox

<histogram-configuration>

<histograms>

<histogram name="HistogramTime" units="milliseconds">
  <owner>person@chromium.org</owner>
  <owner>some-team@chromium.org</owner>
  <summary>A brief description.</summary>
  <details>This is a more thorough description of this histogram.</details>
</histogram>

<histogram name="HistogramEnum" enum="MyEnumType">
  <owner>person@chromium.org</owner>
  <summary>This histogram sports an enum value type.</summary>
</histogram>

</histograms>

<enums>

<enum name="MyEnumType">
  <summary>This is an example enum type, where the values mean little.</summary>
  <int value="1" label="FIRST_VALUE">This is the first value.</int>
  <int value="2" label="SECOND_VALUE">This is the second value.</int>
</enum>

</enums>

<histogram_suffixes_list>

<histogram_suffixes name="BrowserType" separator="_">
  <suffix name="Chrome"/>
  <suffix name="IE"/>
  <suffix name="Firefox"/>
  <affected-histogram name="HistogramEnum"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
"""

import bisect
import copy
import datetime

try:
  import HTMLParser
  html = HTMLParser.HTMLParser()
except ImportError:  # For Py3 compatibility
  import html

import logging
import re
import xml.dom.minidom

BASIC_EMAIL_REGEXP = r'^[\w\-\+\%\.]+\@[\w\-\+\%\.]+$'

OWNER_PLACEHOLDER = (
    'Please list the metric\'s owners. Add more owner tags as needed.')

MAX_HISTOGRAM_SUFFIX_DEPENDENCY_DEPTH = 5

DEFAULT_BASE_HISTOGRAM_OBSOLETE_REASON = (
    'Base histogram. Use suffixes of this histogram instead.')

EXPIRY_DATE_PATTERN = "%Y-%m-%d"
EXPIRY_MILESTONE_RE = re.compile(r'M[0-9]{2,3}\Z')


_ELEMENT_NODE = xml.dom.minidom.Node.ELEMENT_NODE

# List of fields that need to copied by copy.copy to suffixed histogram.
# Other fields may be copied by reference.
_HISTOGRAM_COPY_FIELDS = [
    'fieldtrial_groups', 'fieldtrial_names', 'fieldtrial_labels']


class Error(Exception):
  pass


def IterElementsWithTag(root, tag, depth=-1):
  """Iterates over DOM tree and yields elements matching tag name.

  It's meant to be replacement for `getElementsByTagName`,
  (which does recursive search) but without recursive search
  (nested tags are not supported in histograms files).

  Note: This generator stops going deeper in the tree when it detects
  that there are elements with given tag.

  Args:
    root: XML dom tree.
    tag: Element's tag name.
    depth: Defines how deep in the tree function should search for a match.

  Yields:
    xml.dom.minidom.Node: Element matching criteria.
  """
  if depth == 0 and root.nodeType == _ELEMENT_NODE and root.tagName == tag:
    yield root
    return

  had_tag = False

  skipped = 0

  for child in root.childNodes:
    if child.nodeType == _ELEMENT_NODE and child.tagName == tag:
      had_tag = True
      yield child
    else:
      skipped += 1

  depth -= 1

  if not had_tag and depth != 0:
    for child in root.childNodes:
      for match in IterElementsWithTag(child, tag, depth):
        yield match


def _GetTextFromChildNodes(node):
  """Returns a string concatenation of the text of the given node's children.

  Comments are ignored, consecutive lines of text are joined with a single
  space, and paragraphs are maintained so that long text is more readable on
  dashboards.

  Args:
    node: The DOM Element whose children's text is to be extracted, processed,
      and returned.
  """
  paragraph_break = '\n\n'
  text_parts = []

  for child in node.childNodes:
    if child.nodeType != xml.dom.minidom.Node.COMMENT_NODE:
      child_text = child.toxml()
      if not child_text:
        continue

      # If the given node has the below XML representation, then the text
      # added to the list is 'Some words.\n\nWords.'
      # <tag>
      #   Some
      #   words.
      #
      #   <!--Child comment node.-->
      #
      #   Words.
      # </tag>

      # In the case of the first child text node, raw_paragraphs would store
      # ['\n  Some\n  words.', '  '], and in the case of the second,
      # raw_paragraphs would store ['', '  Words.\n'].
      raw_paragraphs = child_text.split(paragraph_break)

      # In the case of the first child text node, processed_paragraphs would
      # store ['Some words.', ''], and in the case of the second,
      # processed_paragraphs would store ['Words.'].
      processed_paragraphs = [NormalizeString(text)
                              for text in raw_paragraphs
                              if text]
      text_parts.append(paragraph_break.join(processed_paragraphs))

  return ''.join(text_parts).strip()


def NormalizeString(text):
  r"""Replaces all white space sequences with a single space.

  Also, unescapes any HTML escaped characters, e.g. &quot; or &gt;.

  Args:
    text: The string to normalize, '\n\n a \n b&gt;c  '.

  Returns:
    The normalized string 'a b>c'.
  """
  line = ' '.join(text.split())

  # Unescape using default ASCII encoding. Unescapes any HTML escaped character
  # like &quot; etc.
  return html.unescape(line)


def _NormalizeAllAttributeValues(node):
  """Recursively normalizes all tag attribute values in the given tree.

  Args:
    node: The minidom node to be normalized.

  Returns:
    The normalized minidom node.
  """
  if node.nodeType == _ELEMENT_NODE:
    for a in node.attributes.keys():
      node.attributes[a].value = NormalizeString(node.attributes[a].value)

  for c in node.childNodes:
    _NormalizeAllAttributeValues(c)
  return node


def _ExpandHistogramNameWithSuffixes(suffix_name, histogram_name,
                                     histogram_suffixes_node):
  """Creates a new histogram name based on a histogram suffix.

  Args:
    suffix_name: The suffix string to apply to the histogram name. May be empty.
    histogram_name: The name of the histogram. May be of the form
      Group.BaseName or BaseName.
    histogram_suffixes_node: The histogram_suffixes XML node.

  Returns:
    A string with the expanded histogram name.

  Raises:
    Error: if the expansion can't be done.
  """
  if histogram_suffixes_node.hasAttribute('separator'):
    separator = histogram_suffixes_node.getAttribute('separator')
  else:
    separator = '_'

  if histogram_suffixes_node.hasAttribute('ordering'):
    ordering = histogram_suffixes_node.getAttribute('ordering')
  else:
    ordering = 'suffix'
  parts = ordering.split(',')
  ordering = parts[0]
  if len(parts) > 1:
    placement = int(parts[1])
  else:
    placement = 1
  if ordering not in ['prefix', 'suffix']:
    logging.error('ordering needs to be prefix or suffix, value is %s',
                  ordering)
    raise Error()

  if not suffix_name:
    return histogram_name

  if ordering == 'suffix':
    return histogram_name + separator + suffix_name

  # For prefixes, the suffix_name is inserted between the "cluster" and the
  # "remainder", e.g. Foo.BarHist expanded with gamma becomes Foo.gamma_BarHist.
  sections = histogram_name.split('.')
  if len(sections) <= placement:
    logging.error(
        'Prefix histogram_suffixes expansions require histogram names which '
        'include a dot separator. Histogram name is %s, histogram_suffixes is '
        '%s, and placment is %d', histogram_name,
        histogram_suffixes_node.getAttribute('name'), placement)
    raise Error()

  cluster = '.'.join(sections[0:placement]) + '.'
  remainder = '.'.join(sections[placement:])
  return cluster + suffix_name + separator + remainder


def ExtractEnumsFromXmlTree(tree):
  """Extracts all <enum> nodes in the tree into a dictionary."""

  enums = {}
  have_errors = False

  last_name = None
  for enum in IterElementsWithTag(tree, 'enum'):
    name = enum.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Enums %s and %s are not in alphabetical order', last_name,
                    name)
      have_errors = True
    last_name = name

    if name in enums:
      logging.error('Duplicate enum %s', name)
      have_errors = True
      continue

    enum_dict = {}
    enum_dict['name'] = name
    enum_dict['values'] = {}

    nodes = list(IterElementsWithTag(enum, 'int'))

    for int_tag in nodes:
      value_dict = {}
      int_value = int(int_tag.getAttribute('value'))
      if int_value in enum_dict['values']:
        logging.error('Duplicate enum value %d for enum %s', int_value, name)
        have_errors = True
        continue
      value_dict['label'] = int_tag.getAttribute('label')
      value_dict['summary'] = _GetTextFromChildNodes(int_tag)
      enum_dict['values'][int_value] = value_dict

    enum_int_values = sorted(enum_dict['values'].keys())

    last_int_value = None
    for int_tag in nodes:
      int_value = int(int_tag.getAttribute('value'))
      if last_int_value is not None and int_value < last_int_value:
        logging.error('Enum %s int values %d and %d are not in numerical order',
                      name, last_int_value, int_value)
        have_errors = True
        left_item_index = bisect.bisect_left(enum_int_values, int_value)
        if left_item_index == 0:
          logging.warning('Insert value %d at the beginning', int_value)
        else:
          left_int_value = enum_int_values[left_item_index - 1]
          left_label = enum_dict['values'][left_int_value]['label']
          logging.warning('Insert value %d after %d ("%s")', int_value,
                          left_int_value, left_label)
      else:
        last_int_value = int_value

    for summary in IterElementsWithTag(enum, 'summary'):
      enum_dict['summary'] = _GetTextFromChildNodes(summary)
      break

    enums[name] = enum_dict

  return enums, have_errors


def _ExtractOwners(histogram):
  """Extracts owners information from the given histogram element.

  Args:
    histogram: A DOM Element corresponding to a histogram.

  Returns:
    A tuple of owner-related info, e.g. (['alice@chromium.org'], True)

    The first element is a list of the owners' email addresses, excluding the
    owner placeholder string. The second element is a boolean indicating
    whether the histogram has an owner. A histogram whose owner is the owner
    placeholder string has an owner.
  """
  email_pattern = re.compile(BASIC_EMAIL_REGEXP)
  owners = []
  has_owner = False

  for owner_node in IterElementsWithTag(histogram, 'owner', 1):
    child = owner_node.firstChild
    owner_text = (child and child.nodeValue) or ''
    is_email = email_pattern.match(owner_text)

    if owner_text and (is_email or OWNER_PLACEHOLDER in owner_text):
      has_owner = True
      if is_email:
        owners.append(owner_text)

  return owners, has_owner


def _ValidateDateString(date_str):
  """Checks if |date_str| matches 'YYYY-MM-DD'.

  Args:
    date_str: string

  Returns:
    True iff |date_str| matches 'YYYY-MM-DD' format.
  """
  try:
    _ = datetime.datetime.strptime(date_str, EXPIRY_DATE_PATTERN).date()
  except ValueError:
    return False
  return True

def _ValidateMilestoneString(milestone_str):
  """Check if |milestone_str| matches 'M*'."""
  return EXPIRY_MILESTONE_RE.match(milestone_str) is not None

def _ProcessBaseHistogramAttribute(node, histogram_entry):
  if node.hasAttribute('base'):
    is_base = node.getAttribute('base').lower() == 'true'
    histogram_entry['base'] = is_base
    if is_base and 'obsolete' not in histogram_entry:
      histogram_entry['obsolete'] = DEFAULT_BASE_HISTOGRAM_OBSOLETE_REASON


def _ExtractHistogramsFromXmlTree(tree, enums):
  """Extracts all <histogram> nodes in the tree into a dictionary."""

  # Process the histograms. The descriptions can include HTML tags.
  histograms = {}
  have_errors = False
  last_name = None
  for histogram in IterElementsWithTag(tree, 'histogram'):
    name = histogram.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Histograms %s and %s are not in alphabetical order',
                    last_name, name)
      have_errors = True
    last_name = name
    if name in histograms:
      logging.error('Duplicate histogram definition %s', name)
      have_errors = True
      continue
    histograms[name] = histogram_entry = {}

    # Handle expiry attribute.
    if histogram.hasAttribute('expires_after'):
      expiry_str = histogram.getAttribute('expires_after')
      if (expiry_str == "never" or _ValidateMilestoneString(expiry_str) or
          _ValidateDateString(expiry_str)):
        histogram_entry['expires_after'] = expiry_str
      else:
        logging.error(
            'Expiry of histogram %s does not match expected date format ("%s"),'
            ' milestone format (M*), or "never": found %s.', name,
            EXPIRY_DATE_PATTERN, expiry_str)
        have_errors = True

    # Find <owner> tag.
    owners, has_owner = _ExtractOwners(histogram)
    if owners:
      histogram_entry['owners'] = owners

    # Find <summary> tag.
    summary_nodes = list(IterElementsWithTag(histogram, 'summary'))

    if summary_nodes:
      histogram_entry['summary'] = _GetTextFromChildNodes(summary_nodes[0])
    else:
      histogram_entry['summary'] = 'TBD'

    # Find <obsolete> tag.
    obsolete_nodes = list(IterElementsWithTag(histogram, 'obsolete', 1))
    if obsolete_nodes:
      reason = _GetTextFromChildNodes(obsolete_nodes[0])
      histogram_entry['obsolete'] = reason

    # Non-obsolete histograms should provide a <summary>.
    if not obsolete_nodes and not summary_nodes:
      logging.error('histogram %s should provide a <summary>', name)
      have_errors = True

    # Non-obsolete histograms should specify <owner>s.
    if not obsolete_nodes and not has_owner:
      logging.error('histogram %s should specify <owner>s', name)
      have_errors = True

    # Histograms should have either units or enum.
    if (not histogram.hasAttribute('units') and
        not histogram.hasAttribute('enum')):
      logging.error('histogram %s should have either units or enum', name)
      have_errors = True

    # Handle units.
    if histogram.hasAttribute('units'):
      histogram_entry['units'] = histogram.getAttribute('units')

    # Find <details> tag.
    for node in IterElementsWithTag(histogram, 'details'):
      histogram_entry['details'] = _GetTextFromChildNodes(node)
      break

    # Handle enum types.
    if histogram.hasAttribute('enum'):
      enum_name = histogram.getAttribute('enum')
      if enum_name not in enums:
        logging.error('Unknown enum %s in histogram %s', enum_name, name)
        have_errors = True
      else:
        histogram_entry['enum'] = enums[enum_name]

    _ProcessBaseHistogramAttribute(histogram, histogram_entry)

  return histograms, have_errors


def _GetObsoleteReason(node):
  """If the node's histogram is obsolete, returns a string explanation.

  Otherwise, returns None.

  Args:
    node: A DOM Element associated with a histogram.
  """
  for child in node.childNodes:
    if child.localName == 'obsolete':
      # There can be at most 1 obsolete element per node.
      return _GetTextFromChildNodes(child)
  return None


def _UpdateHistogramsWithSuffixes(tree, histograms):
  """Processes <histogram_suffixes> tags and combines with affected histograms.

  The histograms dictionary will be updated in-place by adding new histograms
  created by combining histograms themselves with histogram_suffixes targeting
  these histograms.

  Args:
    tree: XML dom tree.
    histograms: a dictionary of histograms previously extracted from the tree;

  Returns:
    True if any errors were found.
  """
  have_errors = False

  histogram_suffix_tag = 'histogram_suffixes'
  suffix_tag = 'suffix'
  with_tag = 'with-suffix'

  # Verify order of histogram_suffixes fields first.
  last_name = None

  for histogram_suffixes in IterElementsWithTag(
      tree, histogram_suffix_tag, depth=1):
    name = histogram_suffixes.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('histogram_suffixes %s and %s are not in alphabetical '
                    'order', last_name, name)
      have_errors = True
    last_name = name

  # histogram_suffixes can depend on other histogram_suffixes, so we need to be
  # careful. Make a temporary copy of the list of histogram_suffixes to use as a
  # queue. histogram_suffixes whose dependencies have not yet been processed
  # will get relegated to the back of the queue to be processed later.
  reprocess_queue = []

  def GenerateHistogramSuffixes():
    for f in IterElementsWithTag(tree, histogram_suffix_tag):
      yield 0, f
    for r, f in reprocess_queue:
      yield r, f

  for reprocess_count, histogram_suffixes in GenerateHistogramSuffixes():
    # Check dependencies first
    dependencies_valid = True
    affected_histograms = list(IterElementsWithTag(
        histogram_suffixes, 'affected-histogram', 1))
    for affected_histogram in affected_histograms:
      histogram_name = affected_histogram.getAttribute('name')
      if histogram_name not in histograms:
        # Base histogram is missing
        dependencies_valid = False
        missing_dependency = histogram_name
        break
    if not dependencies_valid:
      if reprocess_count < MAX_HISTOGRAM_SUFFIX_DEPENDENCY_DEPTH:
        reprocess_queue.append((reprocess_count + 1, histogram_suffixes))
        continue
      else:
        logging.error('histogram_suffixes %s is missing its dependency %s',
                      histogram_suffixes.getAttribute('name'),
                      missing_dependency)
        have_errors = True
        continue

    # If the suffix group has an obsolete tag, all suffixes it generates inherit
    # its reason.
    group_obsolete_reason = _GetObsoleteReason(histogram_suffixes)

    name = histogram_suffixes.getAttribute('name')
    suffix_nodes = list(IterElementsWithTag(histogram_suffixes, suffix_tag, 1))
    suffix_labels = {}
    for suffix in suffix_nodes:
      suffix_name = suffix.getAttribute('name')
      if not suffix.hasAttribute('label'):
        logging.error('suffix %s in histogram_suffixes %s should have a label',
                      suffix_name, name)
        have_errors = True
      suffix_labels[suffix_name] = suffix.getAttribute('label')
    # Find owners list under current histogram_suffixes tag.
    owners, _ = _ExtractOwners(histogram_suffixes)

    last_histogram_name = None
    for affected_histogram in affected_histograms:
      histogram_name = affected_histogram.getAttribute('name')
      if (last_histogram_name is not None and
          histogram_name.lower() < last_histogram_name.lower()):
        logging.error('Affected histograms %s and %s of histogram_suffixes %s '
                      'are not in alphabetical order', last_histogram_name,
                      histogram_name, name)
        have_errors = True
      last_histogram_name = histogram_name
      with_suffixes = list(IterElementsWithTag(affected_histogram, with_tag, 1))
      if with_suffixes:
        suffixes_to_add = with_suffixes
      else:
        suffixes_to_add = suffix_nodes
      for suffix in suffixes_to_add:
        suffix_name = suffix.getAttribute('name')
        try:
          new_histogram_name = _ExpandHistogramNameWithSuffixes(
              suffix_name, histogram_name, histogram_suffixes)
          if new_histogram_name != histogram_name:
            new_histogram = copy.deepcopy(histograms[histogram_name])
            # Do not copy forward base histogram state to suffixed
            # histograms. Any suffixed histograms that wish to remain base
            # histograms must explicitly re-declare themselves as base
            # histograms.
            if new_histogram.get('base', False):
              del new_histogram['base']
              if (new_histogram.get(
                  'obsolete', '') == DEFAULT_BASE_HISTOGRAM_OBSOLETE_REASON):
                del new_histogram['obsolete']
            histograms[new_histogram_name] = new_histogram

          suffix_label = suffix_labels.get(suffix_name, '')

          histogram_entry = histograms[new_histogram_name]
          for field_name in _HISTOGRAM_COPY_FIELDS:
            histogram_entry.setdefault(field_name, [])

          # TODO(yiyaoliu): Rename these to be consistent with the new naming.
          # It is kept unchanged for now to be it's used by dashboards.
          histogram_entry['fieldtrial_groups'].append(suffix_name)
          histogram_entry['fieldtrial_names'].append(name)
          histogram_entry['fieldtrial_labels'].append(suffix_label)

          # If no owners are added for this histogram-suffixes, it inherits the
          # owners of its parents.
          if owners:
            histogram_entry['owners'] = owners

          # If a suffix has an obsolete node, it's marked as obsolete for the
          # specified reason, overwriting its group's obsoletion reason if the
          # group itself was obsolete as well.
          obsolete_reason = _GetObsoleteReason(suffix)
          if not obsolete_reason:
            obsolete_reason = group_obsolete_reason

          # If the suffix has an obsolete tag, all histograms it generates
          # inherit it.
          if obsolete_reason:
            histogram_entry['obsolete'] = obsolete_reason

          _ProcessBaseHistogramAttribute(suffix, histogram_entry)

        except Error:
          have_errors = True

  return have_errors


def _GetTagSubTree(tree, tag, depth):
  """Returns sub tree with tag element as a root.

  When no element with tag name is found or there are many of them
  original tree is returned.

  Args:
    tree: XML dom tree.
    tag: Element's tag name.
    depth: Defines how deep in the tree function should search for a match.

  Returns:
    xml.dom.minidom.Node: Sub tree (matching criteria) or original one.
  """
  entries = list(IterElementsWithTag(tree, tag, depth))
  if len(entries) == 1:
    tree = entries[0]
  return tree


def ExtractHistogramsFromDom(tree):
  """Computes the histogram names and descriptions from the XML representation.

  Args:
    tree: A DOM tree of XML content.

  Returns:
    a tuple of (histograms, status) where histograms is a dictionary mapping
    histogram names to dictionaries containing histogram descriptions and status
    is a boolean indicating if errros were encountered in processing.
  """
  _NormalizeAllAttributeValues(tree)

  enums_tree = _GetTagSubTree(tree, 'enums', 2)
  histograms_tree = _GetTagSubTree(tree, 'histograms', 2)
  histogram_suffixes_tree = _GetTagSubTree(tree, 'histogram_suffixes_list', 2)
  enums, enum_errors = ExtractEnumsFromXmlTree(enums_tree)
  histograms, histogram_errors = _ExtractHistogramsFromXmlTree(
      histograms_tree, enums)
  update_errors = _UpdateHistogramsWithSuffixes(
      histogram_suffixes_tree, histograms)

  return histograms, enum_errors or histogram_errors or update_errors


def ExtractHistograms(filename):
  """Loads histogram definitions from a disk file.

  Args:
    filename: a file path to load data from.

  Returns:
    a dictionary of histogram descriptions.

  Raises:
    Error: if the file is not well-formatted.
  """
  with open(filename, 'r') as f:
    tree = xml.dom.minidom.parse(f)
    histograms, had_errors = ExtractHistogramsFromDom(tree)
    if had_errors:
      logging.error('Error parsing %s', filename)
      raise Error()
    return histograms


def ExtractNames(histograms):
  return sorted(histograms.keys())

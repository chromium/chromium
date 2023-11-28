# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates enums in histograms.xml file with values read from provided C++ enum.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import io
import logging
import os
import re
import sys

from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util
import histogram_configuration_model


class UserError(Exception):

  @property
  def message(self):
    return self.args[0]


class DuplicatedValue(Exception):
  """Exception raised for duplicated enum values.

  Attributes:
      first_label: First enum label that shares the duplicated enum value.
      second_label: Second enum label that shares the duplicated enum value.
  """
  def __init__(self, first_label, second_label):
    self.first_label = first_label
    self.second_label = second_label


class DuplicatedLabel(Exception):
  """Exception raised for duplicated enum labels.

  Attributes:
      first_value: First enum value that shares the duplicated enum label.
      second_value: Second enum value that shares the duplicated enum label.
  """
  def __init__(self, first_value, second_value):
    self.first_value = first_value
    self.second_value = second_value


def Log(message):
  logging.info(message)


def _CheckForDuplicates(enum_value, label, result):
  """Checks if an enum value or label already exists in the results."""
  if enum_value in result:
    raise DuplicatedValue(result[enum_value], label)
  if label in result.values():
    (dup_value, ) = (k for k, v in result.items() if v == 'label')
    raise DuplicatedLabel(enum_value, dup_value)


def ReadHistogramValues(filename, start_marker, end_marker, strip_k_prefix):
  """Creates a dictionary of enum values, read from a C++ file.

  Args:
      filename: The unix-style path (relative to src/) of the file to open.
      start_marker: A regex that signifies the start of the enum values.
      end_marker: A regex that signifies the end of the enum values.
      strip_k_prefix: Set to True if enum values are declared as kFoo and the
          'k' should be stripped.

  Returns:
      A dictionary from enum value to enum label.

  Raises:
      DuplicatedValue: An error when two enum labels share the same value.
      DuplicatedLabel: An error when two enum values share the same label.
  """
  # Read the file as a list of lines
  with io.open(path_util.GetInputFile(filename)) as f:
    content = f.readlines()

  START_REGEX = re.compile(start_marker)
  ITEM_REGEX = re.compile(r'^(\w+)')
  ITEM_REGEX_WITH_INIT = re.compile(r'(\w+)\s*=\s*(\d*)')
  WRAPPED_INIT = re.compile(r'(\d+)')
  END_REGEX = re.compile(end_marker)

  iterator = iter(content)
  # Find the start of the enum
  for line in iterator:
    if START_REGEX.match(line.strip()):
      break

  enum_value = 0
  result = {}
  for line in iterator:
    line = line.strip()
    # Exit condition: we reached last enum value
    if END_REGEX.match(line):
      break
    # Inside enum: generate new xml entry
    m = ITEM_REGEX_WITH_INIT.match(line)
    if m:
      label = m.group(1)
      if m.group(2):
        enum_value = int(m.group(2))
      else:
        # Enum name is so long that the value wrapped to the next line
        next_line = next(iterator).strip()
        enum_value = int(WRAPPED_INIT.match(next_line).group(1))
    else:
      m = ITEM_REGEX.match(line)
      if m:
        label = m.group(1)
      else:
        continue
    if strip_k_prefix:
      assert label.startswith('k'), "Enum " + label + " should start with 'k'."
      label = label[1:]
    _CheckForDuplicates(enum_value, label, result)
    result[enum_value] = label
    enum_value += 1
  return result


def _CreateEnumItemNode(document, value, label):
  """Creates an int element to append to an enum."""
  item_node = document.createElement('int')
  item_node.attributes['value'] = str(value)
  item_node.attributes['label'] = label
  return item_node


def _UpdateHistogramDefinitions(histogram_enum_name, source_enum_values,
                                source_enum_path, caller_script_name, document):
  """Updates the enum node named |histogram_enum_name| based on the definition
  stored in |source_enum_values|. Existing items for which |source_enum_values|
  doesn't contain any corresponding data will be preserved. |source_enum_path|
  and |caller_script_name| will be used to insert a comment.
  """
  # Get a dom of <enum name=|histogram_enum_name| ...> node in |document|.
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == histogram_enum_name:
      break
  else:
    raise UserError('No {0} enum node found'.format(histogram_enum_name))

  new_item_nodes = {}
  new_comments = []

  # Add a "Generated from (...)" comment.
  new_comments.append(
      document.createComment(
          ' Generated from {0}.'.format(source_enum_path).replace('\\', '/') +
          ('\nCalled by {0}.'.format(caller_script_name
                                     ) if caller_script_name else '')))

  # Create item nodes for each of the enum values.
  for value, label in source_enum_values.items():
    new_item_nodes[value] = _CreateEnumItemNode(document, value, label)

  # Scan existing nodes in |enum_node| for old values and preserve them.
  # - Preserve comments other than the 'Generated from' comment. NOTE:
  #   this does not preserve the order of the comments in relation to the
  #   old values.
  # - Drop anything else.
  SOURCE_COMMENT_REGEX = re.compile('^ Generated from ')
  for child in enum_node.childNodes:
    if child.nodeName == 'int':
      value = int(child.attributes['value'].value)
      if value not in source_enum_values:
        new_item_nodes[value] = child
    # Preserve existing non-generated comments.
    elif (child.nodeType == minidom.Node.COMMENT_NODE and
          SOURCE_COMMENT_REGEX.match(child.data) is None):
      new_comments.append(child)

  # Update |enum_node|. First, remove everything existing.
  while enum_node.hasChildNodes():
    enum_node.removeChild(enum_node.lastChild)

  # Add comments at the top.
  for comment in new_comments:
    enum_node.appendChild(comment)

  # Add in the new enums.
  for value in sorted(new_item_nodes.keys()):
    enum_node.appendChild(new_item_nodes[value])


def _GetOldAndUpdatedXml(enums_xml_path, histogram_enum_name,
                         source_enum_values, source_enum_path,
                         caller_script_name):
  """Reads old histogram from |histogram_enum_name| from |enums_xml_path|, and
  calculates new histogram from |source_enum_values| from |source_enum_path|,
  and returns both in XML format.
  """
  Log('Reading existing histograms from "{0}".'.format(enums_xml_path))
  with io.open(enums_xml_path, 'r', encoding='utf-8') as f:
    histograms_doc = minidom.parse(f)
    f.seek(0)
    xml = f.read()

  Log('Comparing histograms enum with new enum definition.')
  _UpdateHistogramDefinitions(histogram_enum_name, source_enum_values,
                              source_enum_path, caller_script_name,
                              histograms_doc)

  new_xml = histogram_configuration_model.PrettifyTree(histograms_doc)
  return (xml, new_xml)


def CheckPresubmitErrors(enums_xml_path,
                         histogram_enum_name,
                         update_script_name,
                         source_enum_path,
                         start_marker,
                         end_marker,
                         strip_k_prefix=False,
                         histogram_value_reader=ReadHistogramValues):
  """Extracts histogram enum values from a source file and checks for
  violations.

  Enum values are extracted from |source_enum_path| using
  |histogram_value_reader| function. The following presubmit violations are then
  checked:
    1. Failure to update histograms.xml to match
    2. Introduction of duplicate values

  Args:
      enums_xml_path: Src-relative path to the enums.xml file to update.
      histogram_enum_name: The name of the XML <enum> attribute to update.
      update_script_name: The name of an update script to run to update the UMA
          mappings for the enum.
      source_enum_path: A unix-style path, relative to src/, giving
          the source file from which to read the enum.
      start_marker: A regular expression that matches the start of the C++ enum.
      end_marker: A regular expression that matches the end of the C++ enum.
      strip_k_prefix: Set to True if enum values are declared as kFoo and the
          'k' should be stripped.
      histogram_value_reader: A reader function that takes four arguments
          (source_path, start_marker, end_marker, strip_k_prefix), and returns a
          list of strings of the extracted enum names. The default is
          ReadHistogramValues(), which parses the values out of an enum defined
          in a C++ source file.


  Returns:
      A string with presubmit failure description, or None (if no failures).
  """
  Log('Reading histogram enum definition from "{0}".'.format(source_enum_path))
  try:
    source_enum_values = histogram_value_reader(source_enum_path, start_marker,
                                                end_marker, strip_k_prefix)
  except DuplicatedValue as duplicated_values:
    return ('%s enum has been updated and there exist '
            'duplicated values between (%s) and (%s)' %
            (histogram_enum_name, duplicated_values.first_label,
             duplicated_values.second_label))
  except DuplicatedLabel as duplicated_labels:
    return ('%s enum has been updated and there exist '
            'duplicated labels between (%s) and (%s)' %
            (histogram_enum_name, duplicated_labels.first_value,
             duplicated_labels.second_value))

  (xml, new_xml) = _GetOldAndUpdatedXml(path_util.GetInputFile(enums_xml_path),
                                        histogram_enum_name, source_enum_values,
                                        source_enum_path, update_script_name)
  if xml != new_xml:
    return ('%s enum has been updated and the UMA mapping needs to be '
            'regenerated. Please run %s in src/tools/metrics/histograms/ to '
            'update the mapping.' % (histogram_enum_name, update_script_name))

  return None


def UpdateHistogramFromDict(enums_xml_path, histogram_enum_name,
                            source_enum_values, source_enum_path,
                            caller_script_name):
  """Updates an enums.xml file with values from a {value: 'key'} dictionary.

  A comment is added to enums.xml citing that the values in
  |histogram_enum_name| were sourced from |source_enum_path|, requested by
  |caller_script_name|.

  Args:
      enums_xml_path: Src-relative path to the enums.xml file to update.
      histogram_enum_name: The name of the XML <enum> attribute to update.
      source_enum_values: The {value: 'key'} dictionary containing enum values.
      source_enum_path: A unix-style path, relative to src/, giving
          the C++ header file from which to read the enum.
      caller_script_name: Name of the calling script.
  """
  enums_xml_path = path_util.GetInputFile(enums_xml_path)
  (xml, new_xml) = _GetOldAndUpdatedXml(enums_xml_path, histogram_enum_name,
                                        source_enum_values, source_enum_path,
                                        caller_script_name)
  with io.open(enums_xml_path, 'w', encoding='utf-8', newline='') as f:
    f.write(new_xml)

  Log('Done.')


def UpdateHistogramEnum(enums_xml_path,
                        histogram_enum_name,
                        source_enum_path,
                        start_marker,
                        end_marker,
                        strip_k_prefix=False,
                        calling_script=None):
  """Reads a C++ enum from a .h file and updates histograms.xml to match.

  Args:
      enums_xml_path: Src-relative path to the enums.xml file to update.
      histogram_enum_name: The name of the XML <enum> attribute to update.
      source_enum_path: A unix-style path, relative to src/, giving
          the C++ header file from which to read the enum.
      start_marker: A regular expression that matches the start of the C++ enum.
      end_marker: A regular expression that matches the end of the C++ enum.
      strip_k_prefix: Set to True if enum values are declared as kFoo and the
          'k' should be stripped.
  """
  Log('Reading histogram enum definition from "{0}".'.format(source_enum_path))
  source_enum_values = ReadHistogramValues(source_enum_path,
      start_marker, end_marker, strip_k_prefix)

  UpdateHistogramFromDict(enums_xml_path, histogram_enum_name,
                          source_enum_values, source_enum_path, calling_script)

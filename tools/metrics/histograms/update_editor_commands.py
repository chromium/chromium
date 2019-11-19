# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates MappedEditingCommands enum in histograms.xml file with values read
 from EditorCommand.cpp.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

from __future__ import print_function

import logging
import os
import re
import sys

from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
from diff_util import PromptUserToAcceptDiff
import path_util

import histogram_paths
import histograms_print_style


ENUMS_PATH = histogram_paths.ENUMS_XML
ENUM_NAME = 'MappedEditingCommands'

EDITOR_COMMAND_CPP = 'third_party/blink/renderer/core/editing/editor_command.cc'
ENUM_START_MARKER = "^    static const CommandEntry commands\[\] = {"
ENUM_END_MARKER = "^    };"


class UserError(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @property
  def message(self):
    return self.args[0]


def ReadHistogramValues(filename):
  """Returns a list of pairs (label, value) corresponding to HistogramValue.

  Reads the EditorCommand.cpp file, locates the
  HistogramValue enum definition and returns a pair for each entry.
  """

  # Read the file as a list of lines
  with open(path_util.GetInputFile(filename)) as f:
    content = f.readlines()

  # Locate the enum definition and collect all entries in it
  inside_enum = False # We haven't found the enum definition yet
  result = []
  for line in content:
    if inside_enum:
      # Exit condition: we reached last enum value
      if re.match(ENUM_END_MARKER, line):
        inside_enum = False
      else:
        # Inside enum: generate new xml entry
        m = re.match("^{ \"([\w]+)\", \{([\w]+)", line.strip())
        if m:
          result.append((m.group(1), int(m.group(2))))
    else:
      if re.match(ENUM_START_MARKER, line):
        inside_enum = True
        enum_value = 0 # Start at 'UNKNOWN'
  return sorted(result, key=lambda pair: pair[1])


def UpdateHistogramDefinitions(histogram_values, document):
  """Sets the children of <enum name="ExtensionFunctions" ...> node in
  |document| to values generated from policy ids contained in
  |policy_templates|.

  Args:
    histogram_values: A list of pairs (label, value) defining each extension
                      function
    document: A minidom.Document object representing parsed histogram
              definitions XML file.

  """
  # Find ExtensionFunctions enum.
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == ENUM_NAME:
      extension_functions_enum_node = enum_node
      break
  else:
    raise UserError('No policy enum node found')

  # Remove existing values.
  while extension_functions_enum_node.hasChildNodes():
    extension_functions_enum_node.removeChild(
      extension_functions_enum_node.lastChild)

  # Add a "Generated from (...)" comment
  comment = ' Generated from {0} '.format(EDITOR_COMMAND_CPP)
  extension_functions_enum_node.appendChild(document.createComment(comment))

  # Add values generated from policy templates.
  for (label, value) in histogram_values:
    node = document.createElement('int')
    node.attributes['value'] = str(value)
    node.attributes['label'] = label
    extension_functions_enum_node.appendChild(node)


def Log(message):
  logging.info(message)


def main():
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  Log('Reading histogram enum definition from "%s".' % EDITOR_COMMAND_CPP)
  histogram_values = ReadHistogramValues(EDITOR_COMMAND_CPP)

  Log('Reading existing histograms from "%s".' % (ENUMS_PATH))
  with open(ENUMS_PATH, 'rb') as f:
    histograms_doc = minidom.parse(f)
    f.seek(0)
    xml = f.read()

  Log('Comparing histograms enum with new enum definition.')
  UpdateHistogramDefinitions(histogram_values, histograms_doc)

  Log('Writing out new histograms file.')
  new_xml = histograms_print_style.GetPrintStyle().PrettyPrintXml(
      histograms_doc)
  if PromptUserToAcceptDiff(xml, new_xml, 'Is the updated version acceptable?'):
    with open(ENUMS_PATH, 'wb') as f:
      f.write(new_xml)

  Log('Done.')


if __name__ == '__main__':
  main()

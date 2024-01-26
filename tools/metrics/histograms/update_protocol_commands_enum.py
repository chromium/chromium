#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import hashlib
import ctypes

from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

sys.path.append(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'third_party',
                 'inspector_protocol'))
import pdl

import update_histogram_enum
import histogram_paths


DEV_ENUMS_XML_PATH = 'tools/metrics/histograms/metadata/dev/enums.xml'


def GetCommandUMAId(cdp_command):
  """Generate a hash consistent with GetCommandUMAId() in ChromeDevToolsSession.

   Args:
    cdp_command: A string containing a CDP command.

   Returns:
    The hashed value for the CDP command.
  """
  digest = hashlib.md5(cdp_command.encode('utf-8')).hexdigest()
  first_eight_bytes = digest[:16]
  long_value = int(first_eight_bytes, 16)
  signed_32bit = ctypes.c_int(long_value).value
  return signed_32bit


def ParseProtocolCommandsFromPDL(file_path):
  """Parses a PDL file and returns a dictionary of all its commands and their
  hashes.

   Args:
    file_path: The path of the PDL file.

   Returns:
    A dictionary with the hashes as keys and the CDP commands as values.
  """
  file_name = path_util.GetInputFile(file_path)
  input_file = open(file_name, "r")
  pdl_string = input_file.read()
  protocol = pdl.loads(pdl_string, file_name, False)
  input_file.close()

  result = {}
  for domain in protocol["domains"]:
    if "commands" in domain:
      for command in domain["commands"]:
        command_name = domain["domain"] + "." + command["name"]
        hashed_command = GetCommandUMAId(command_name)
        if (hashed_command in result):
          print('Hash collision between "{}" and "{}" in {} when '\
          'generating CDPCommands for enums.xml'
          .format(result[hashed_command], command_name, file_path))
        result[hashed_command] = command_name

  return result


def ParseProtocolCommandsFromXML():
  """Parses the 'CDPCommands' enum in enums.xml.

   Returns:
    A dictionary with the hashes as keys and the CDP commands as values.
  """
  document = minidom.parse(path_util.GetInputFile(DEV_ENUMS_XML_PATH))
  result = {}

  # Get DOM of the <enum name="CDPCommands"> node.
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == 'CDPCommands':
      break
  else:
    raise Exception('CDPCommands enum node not found in enums.xml')

  for child in enum_node.childNodes:
    if child.nodeName == 'int':
      enum_value = int(child.attributes['value'].value)
      enum_label = str(child.attributes['label'].value)
      result[enum_value] = enum_label

  return result


def CheckDictsForCollisions(first, second):
  """Compares 2 dictionaries and prints an error message for each key which
  is contained in both dics for which the corresponding value differs in the
  2 dicts.

   Args:
    first: A dictionary.
    second: A dictionary.
  """
  for hashedValue in first.keys():
    if (hashedValue in second and second[hashedValue] != first[hashedValue]):
      print(
        'Hash collision between "{}" and "{}" when generating CDPCommands '\
        'for enums.xml'
        .format(first[hashedValue], second[hashedValue]))


def MaybeUpdateEnumFromFile(file_path):
  """Gets the results of parsing a pdl file and of enums.xml, and updates
  enums.xml if necessary.

   Args:
    file_path: Path of the pdl file to be parsed.
  """
  print('Parsing {}'.format(file_path))
  pdl_dict = ParseProtocolCommandsFromPDL(file_path)
  xml_dict = ParseProtocolCommandsFromXML()
  CheckDictsForCollisions(pdl_dict, xml_dict)
  files_for_enum_comment = '*.pdl files'
  update_histogram_enum.UpdateHistogramFromDict(DEV_ENUMS_XML_PATH,
                                                'CDPCommands', pdl_dict,
                                                files_for_enum_comment,
                                                os.path.basename(__file__))


def main():
  """Checks that the 'CDPCommands' enum in enums.xml matches the content of
  the various pdl protocol definition files and updates the enum if necessary.
  """

  pdl_file_paths = [
      'third_party/blink/public/devtools_protocol/browser_protocol.pdl',
      'v8/include/js_protocol.pdl', 'chrome/browser/devtools/cros_protocol.pdl',
      'components/viz/common/debugger/viz_debugger.pdl',
      'content/browser/native_profiling.pdl'
  ]
  for pdl_file_path in pdl_file_paths:
    MaybeUpdateEnumFromFile(pdl_file_path)


if __name__ == '__main__':
  main()

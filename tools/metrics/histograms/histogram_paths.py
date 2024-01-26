#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Paths to description XML files in this directory.

When executed, updates the `histograms_xml_files.gni` file to correspond to the
histograms.xml and enums.xml files that exist.
"""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util


_XML_FILE_NAMES = ['histograms.xml', 'enums.xml', 'histogram_suffixes_list.xml']


def _FindXmlFiles():
  """Gets a list relative path to all metrics XML files under metadata/."""
  files = []
  for dir_name, _, file_list in os.walk(PATH_TO_METADATA_DIR):
    for filename in file_list:
      if filename in _XML_FILE_NAMES:
        # Compute the relative path of the histograms xml file.
        file_path = os.path.relpath(os.path.join(dir_name, filename),
                                    PATH_TO_METADATA_DIR)
        files.append(
            os.path.join('tools/metrics/histograms/metadata',
                         file_path).replace(os.sep, '/').lower())
  return sorted(files)


ENUMS_XML_RELATIVE = 'tools/metrics/histograms/enums.xml'
# The absolute path to the metadata folder.
PATH_TO_METADATA_DIR = path_util.GetInputFile(
    'tools/metrics/histograms/metadata')
# In the middle state, histogram paths include both the large histograms.xml
# file as well as the split up files.
# TODO: Improve on the current design to avoid calling `os.walk()` at the time
# of module import.
HISTOGRAMS_XMLS_RELATIVE = ([
    'tools/metrics/histograms/histograms.xml',
] + _FindXmlFiles())
ALL_XMLS_RELATIVE = [ENUMS_XML_RELATIVE] + HISTOGRAMS_XMLS_RELATIVE

HISTOGRAMS_PREFIX_LIST = [
    os.path.basename(os.path.dirname(f)) for f in HISTOGRAMS_XMLS_RELATIVE
]

ENUMS_XML = path_util.GetInputFile(ENUMS_XML_RELATIVE)
UKM_XML = path_util.GetInputFile('tools/metrics/ukm/ukm.xml')
HISTOGRAMS_XMLS = [path_util.GetInputFile(f) for f in HISTOGRAMS_XMLS_RELATIVE]
ALL_XMLS = [path_util.GetInputFile(f) for f in ALL_XMLS_RELATIVE]

ALL_TEST_XMLS_RELATIVE = [
    'tools/metrics/histograms/test_data/enums.xml',
    'tools/metrics/histograms/test_data/enums2.xml',
    'tools/metrics/histograms/test_data/histograms.xml',
    'tools/metrics/histograms/test_data/histogram_suffixes_list.xml',
    'tools/metrics/histograms/test_data/ukm.xml',
]
ALL_TEST_XMLS = [path_util.GetInputFile(f) for f in ALL_TEST_XMLS_RELATIVE]
(TEST_ENUMS_XML, TEST_ENUMS2_XML, TEST_HISTOGRAMS_XML, TEST_SUFFIXES_XML,
 TEST_UKM_XML) = ALL_TEST_XMLS

TEST_XML_WITH_COMPONENTS_RELATIVE = (
    'tools/metrics/histograms/test_data/components/histograms.xml')
TEST_XML_WITH_COMPONENTS = path_util.GetInputFile(
    TEST_XML_WITH_COMPONENTS_RELATIVE)

# The path to the `histograms_xml_files.gni` file.
_HISTOGRAMS_XML_FILES_GNI = path_util.GetInputFile(
    'tools/metrics/histograms/histograms_xml_files.gni')

_GNI_LINE_PREFIX = '  "//'
_GNI_LINE_SUFFIX = '",\n'


def _GenerateHistogramsXmlGniContent():
  """Generates the contents for the _HISTOGRAMS_XML_FILES_GNI file."""
  content = 'histograms_xml_files = [\n'
  for path in ALL_XMLS_RELATIVE:
    content += _GNI_LINE_PREFIX
    content += path.replace(os.sep, '/')
    content += _GNI_LINE_SUFFIX
  content += ']\n'
  return content


def UpdateHistogramsXmlGniFile():
  """Updates the _HISTOGRAMS_XML_FILES_GNI file."""
  with open(_HISTOGRAMS_XML_FILES_GNI, 'w+') as f:
    f.write(_GenerateHistogramsXmlGniContent())


def ValidateHistogramsGniFile():
  """Returns true if _HISTOGRAMS_XML_FILES_GNI file is up to date."""
  with open(_HISTOGRAMS_XML_FILES_GNI, 'r') as f:
    return _GenerateHistogramsXmlGniContent() == f.read()


def main():
  UpdateHistogramsXmlGniFile()

if __name__ == '__main__':
  main()

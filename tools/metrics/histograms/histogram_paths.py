#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Paths to description XML files in this directory."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util


def _FindHistogramsXmlFiles():
  """Gets a list relative path to all histograms xmls under metadata/."""
  files = []
  for dir_name, _, file_list in os.walk(PATH_TO_METADATA_DIR):
    for filename in file_list:
      if (filename == 'histograms.xml'
          or filename == 'histogram_suffixes_list.xml'):
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
HISTOGRAMS_XMLS_RELATIVE = (['tools/metrics/histograms/histograms.xml'] +
                            _FindHistogramsXmlFiles())
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
    'tools/metrics/histograms/test_data/histograms.xml',
    'tools/metrics/histograms/test_data/histogram_suffixes_list.xml',
    'tools/metrics/histograms/test_data/ukm.xml',
]
ALL_TEST_XMLS = [path_util.GetInputFile(f) for f in ALL_TEST_XMLS_RELATIVE]
(TEST_ENUMS_XML, TEST_HISTOGRAMS_XML, TEST_SUFFIXES_XML,
 TEST_UKM_XML) = ALL_TEST_XMLS

TEST_XML_WITH_COMPONENTS_RELATIVE = (
    'tools/metrics/histograms/test_data/components/histograms.xml')
TEST_XML_WITH_COMPONENTS = path_util.GetInputFile(
    TEST_XML_WITH_COMPONENTS_RELATIVE)

# The path to the `histogram_index` file.
HISTOGRAMS_INDEX = path_util.GetInputFile(
    'tools/metrics/histograms/histograms_index.txt')


def main():
  with open(HISTOGRAMS_INDEX, 'w+') as f:
    # Force all OSes to use '/' as the separator.
    f.write(''.join([
        path.replace(os.sep, '/') + '\n' for path in HISTOGRAMS_XMLS_RELATIVE
    ]))


if __name__ == '__main__':
  # Update the `histograms_index` file whenever histograms paths are updated.
  # This file records all currently existing histograms.xml paths.
  main()

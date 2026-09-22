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
from typing import Iterable, NamedTuple

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.path_util as path_util

_HISTOGRAM_XML_FILE_NAMES = ['histograms.xml']
_ENUMS_XML_FILE_NAMES = ['enums.xml']


def _FindXmlFiles(filenames):
  """Gets a list relative path to all metrics XML files under metadata/."""
  files = []
  for dir_name, _, file_list in os.walk(PATH_TO_METADATA_DIR):
    for filename in file_list:
      if filename in filenames:
        # Compute the relative path of the histograms xml file.
        file_path = os.path.relpath(
          os.path.join(dir_name, filename), PATH_TO_METADATA_DIR
        )
        files.append(
          os.path.join('tools/metrics/histograms/metadata', file_path)
          .replace(os.sep, '/')
          .lower()
        )
  return sorted(files)


# The absolute path to the metadata folder.
PATH_TO_METADATA_DIR = path_util.GetInputFile(
  'tools/metrics/histograms/metadata'
)
_ENUMS_XML_RELATIVE = [
  'tools/metrics/histograms/enums.xml',
] + _FindXmlFiles(_ENUMS_XML_FILE_NAMES)
_VARIANTS_XML_RELATIVE = [
  'tools/metrics/histograms/variants.xml',
]
_HISTOGRAMS_XMLS_RELATIVE = _FindXmlFiles(_HISTOGRAM_XML_FILE_NAMES)
ALL_XMLS_RELATIVE = (
  _ENUMS_XML_RELATIVE + _VARIANTS_XML_RELATIVE + _HISTOGRAMS_XMLS_RELATIVE
)

HISTOGRAMS_PREFIX_LIST = [
  os.path.basename(os.path.dirname(f)) for f in _HISTOGRAMS_XMLS_RELATIVE
]

ENUMS_XMLS = [path_util.GetInputFile(f) for f in _ENUMS_XML_RELATIVE]
UKM_XML = path_util.GetInputFile('tools/metrics/ukm/ukm.xml')
DWA_XML = path_util.GetInputFile('tools/metrics/private_metrics/dwa.xml')
HISTOGRAMS_XMLS = [path_util.GetInputFile(f) for f in _HISTOGRAMS_XMLS_RELATIVE]
VARIANTS_XMLS = [path_util.GetInputFile(f) for f in _VARIANTS_XML_RELATIVE]
ALL_XMLS = [path_util.GetInputFile(f) for f in ALL_XMLS_RELATIVE]


class _PlatformXmls(NamedTuple):
  """Describes a platform's metrics XML directories and GN build condition."""

  # Short identifier used for the generated GNI list name (e.g. 'android').
  name: str
  # GN boolean condition guarding this platform (e.g. 'is_android').
  gn_condition: str
  # Repo-relative directory prefixes containing this platform's XML files.
  prefixes: tuple[str, ...]

  def FilterPaths(self, paths: Iterable[str]) -> list[str]:
    """Returns paths from |paths| that belong to this platform."""
    return [p for p in paths if p.startswith(self.prefixes)]


_PLATFORM_XML_DIRS = (
  _PlatformXmls(
    'android', 'is_android', ('tools/metrics/histograms/metadata/android/',)
  ),
  _PlatformXmls(
    'chromeos',
    'is_chromeos',
    (
      'tools/metrics/histograms/metadata/ash/',
      'tools/metrics/histograms/metadata/chromeos/',
    ),
  ),
  _PlatformXmls('ios', 'is_ios', ('tools/metrics/histograms/metadata/ios/',)),
  _PlatformXmls(
    'linux', 'is_linux', ('tools/metrics/histograms/metadata/linux/',)
  ),
  _PlatformXmls('mac', 'is_mac', ('tools/metrics/histograms/metadata/mac/',)),
  _PlatformXmls(
    'windows', 'is_win', ('tools/metrics/histograms/metadata/windows/',)
  ),
)
_ALL_PLATFORM_PREFIXES = tuple(
  prefix for platform in _PLATFORM_XML_DIRS for prefix in platform.prefixes
)
PLATFORM_HISTOGRAMS_XMLS = tuple(
  tuple(
    path_util.GetInputFile(f)
    for f in platform.FilterPaths(_HISTOGRAMS_XMLS_RELATIVE)
  )
  for platform in _PLATFORM_XML_DIRS
)

ALL_TEST_XMLS_RELATIVE = [
  'tools/metrics/histograms/test_data/enums.xml',
  'tools/metrics/histograms/test_data/enums2.xml',
  'tools/metrics/histograms/test_data/histograms.xml',
  'tools/metrics/histograms/test_data/ukm.xml',
]
ALL_TEST_XMLS = [path_util.GetInputFile(f) for f in ALL_TEST_XMLS_RELATIVE]
(
  TEST_ENUMS_XML,
  TEST_ENUMS2_XML,
  TEST_HISTOGRAMS_XML,
  TEST_UKM_XML,
) = ALL_TEST_XMLS

TEST_XML_WITH_COMPONENTS_RELATIVE = (
  'tools/metrics/histograms/test_data/components/histograms.xml'
)
TEST_XML_WITH_COMPONENTS = path_util.GetInputFile(
  TEST_XML_WITH_COMPONENTS_RELATIVE
)

# The path to the `histograms_xml_files.gni` file.
_HISTOGRAMS_XML_FILES_GNI = path_util.GetInputFile(
  'tools/metrics/histograms/histograms_xml_files.gni'
)

_GNI_LINE_PREFIX = '  "//'
_GNI_LINE_SUFFIX = '",\n'


def _FormatGniList(name: str, paths: Iterable[str]) -> str:
  lines = [f'{name} = [\n']
  for path in sorted(paths):
    normalized = path.replace(os.sep, '/')
    lines.append(f'{_GNI_LINE_PREFIX}{normalized}{_GNI_LINE_SUFFIX}')
  lines.append(']\n')
  return ''.join(lines)


def _GenerateHistogramsXmlGniContent() -> str:
  """Generates the contents for the _HISTOGRAMS_XML_FILES_GNI file."""
  common_enum_files = [
    p for p in _ENUMS_XML_RELATIVE if not p.startswith(_ALL_PLATFORM_PREFIXES)
  ]
  common_histogram_files = [
    p
    for p in (_HISTOGRAMS_XMLS_RELATIVE + _VARIANTS_XML_RELATIVE)
    if not p.startswith(_ALL_PLATFORM_PREFIXES)
  ]

  sections = [
    '# Note: The contents of this file are auto-generated from the script at\n'
    '# //tools/metrics/histograms/histogram_paths.py.\n\n'
    + _FormatGniList('common_enums_xml_files', common_enum_files),
    _FormatGniList('histograms_and_variants_xml_files', common_histogram_files),
  ]
  for platform in _PLATFORM_XML_DIRS:
    platform_files = platform.FilterPaths(ALL_XMLS_RELATIVE)
    sections.append(
      _FormatGniList(f'{platform.name}_xml_files', platform_files)
    )

  sections.append(
    'histograms_xml_files =\n'
    '    common_enums_xml_files + histograms_and_variants_xml_files\n\n'
    '# Official Windows builds generate the merged histograms.xml file\n'
    '# consumed by server-side metrics infrastructure, so they must include\n'
    '# XML files from all platforms as inputs.\n'
    '_include_all_platforms = is_official_build && is_win\n'
  )

  for platform in _PLATFORM_XML_DIRS:
    platform_histograms = sorted(
      f'//{p}' for p in platform.FilterPaths(_HISTOGRAMS_XMLS_RELATIVE)
    )
    if len(platform_histograms) == 1:
      hist_append = (
        '  histograms_and_variants_xml_files +=\n'
        f'      [ "{platform_histograms[0]}" ]\n'
      )
    else:
      items = ''.join(f'    "{h}",\n' for h in platform_histograms)
      hist_append = f'  histograms_and_variants_xml_files += [\n{items}  ]\n'
    sections.append(
      f'if ({platform.gn_condition} || _include_all_platforms) {{\n'
      f'  histograms_xml_files += {platform.name}_xml_files\n'
      f'{hist_append}'
      f'}}\n'
    )

  return '\n'.join(sections)


def UpdateHistogramsXmlGniFile() -> None:
  """Updates the _HISTOGRAMS_XML_FILES_GNI file."""
  with open(_HISTOGRAMS_XML_FILES_GNI, 'w+') as f:
    f.write(_GenerateHistogramsXmlGniContent())


def ValidateHistogramsGniFile() -> bool:
  """Returns true if _HISTOGRAMS_XML_FILES_GNI file is up to date."""
  with open(_HISTOGRAMS_XML_FILES_GNI, 'r') as f:
    return _GenerateHistogramsXmlGniContent() == f.read()


def main():
  UpdateHistogramsXmlGniFile()


if __name__ == '__main__':
  main()

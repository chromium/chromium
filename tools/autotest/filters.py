# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

import utils.command_util as command
import utils.constants as const

from pathlib import Path


def BuildTestFilter(filenames: list[str], line: int | None) -> str:
  java_files: list[str] = [f for f in filenames if f.endswith('.java')]
  # TODO(crbug.com/434009870): Support EarlGrey tests, which don't use
  # Googletest's macros or pascal case naming convention.
  cc_files: list[str] = [
      f for f in filenames if f.endswith('.cc') or f.endswith('_unittest.mm')
  ]
  filters: list[str] = []
  if java_files:
    filters.append(BuildJavaTestFilter(java_files))
  if cc_files:
    filters.append(BuildCppTestFilter(cc_files, line))
  for regex, gtest_filter in const.SPECIAL_TEST_FILTERS:
    if any(True for f in filenames if regex.match(f)):
      filters.append(gtest_filter)
      break
  return ':'.join(filters)


def BuildCppTestFilter(filenames: list[str], line: int | None) -> str:
  make_filter_command: list[str | Path] = [
      sys.executable, const.SRC_DIR / 'tools' / 'make_gtest_filter.py'
  ]
  if line:
    make_filter_command += ['--line', str(line)]
  else:
    make_filter_command += ['--class-only']
  make_filter_command += filenames
  return command.RunCommand(make_filter_command).strip()


def BuildJavaTestFilter(filenames: list[str]) -> str:
  return ':'.join('*.{}*'.format(os.path.splitext(os.path.basename(f))[0])
                  for f in filenames)


def BuildPrefMappingTestFilter(filenames: list[str]) -> str | None:
  mapping_files: list[str] = [
      f for f in filenames if const.PREF_MAPPING_FILE_REGEX.match(f)
  ]
  if not mapping_files:
    return None
  names_without_extension: list[str] = [Path(f).stem for f in mapping_files]
  return ':'.join(names_without_extension)

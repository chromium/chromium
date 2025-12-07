#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import ctypes
import os
import re
import subprocess
import sys
import typing

# Import the shared codegen library for its hashing function, which is the
# same hashing function as used for flag names.
sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, 'common'))
import codegen_shared

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, 'python', 'google'))
import path_utils

import pretty_print


def get_entries_from_unit_test(outdir: str) -> typing.List[str]:
  """Returns `<int>` entries reported missing by the 'CheckHistograms' unittest.
  """
  subprocess.run(['autoninja', '-C', outdir, 'unit_tests'])
  run_test_command = subprocess.run([
      os.path.join(outdir, 'unit_tests'),
      '--gtest_filter=AboutFlagsHistogramTest.CheckHistograms'
  ],
                                    capture_output=True,
                                    text=True)
  return re.findall('<int [^>]*>', run_test_command.stdout)


def get_entries_from_feature_string(feature: str) -> typing.List[str]:
  """Generates entries for `feature`."""
  entries = []
  for suffix in ['disabled', 'enabled']:
    label = f'{feature}:{suffix}'
    value_64 = codegen_shared.HashName(label)
    value_32 = ctypes.c_int32(value_64).value
    entries.append(f'<int value="{value_32}" label="{label}"/>')
  return entries


def add_entries_to_xml(enums_xml: str, entries: typing.List[str]) -> str:
  """Adds each of `entries` to `enums_xml` and pretty prints it."""
  # Only add entries not already present.
  entries = [entry for entry in entries if enums_xml.find(entry) == -1]
  if entries:
    find_text = '<enum name="LoginCustomFlags">'
    find_index = enums_xml.find(find_text)
    if find_index == -1:
      raise Exception(f'Missing {find_text} in enums.xml.')
    find_index += len(find_text)
    enums_xml = (enums_xml[:find_index] + ' '.join(entries) +
                 enums_xml[find_index:])
  return pretty_print.PrettyPrintEnums(enums_xml)


def main():
  """Generates and formats flag enums.

  Args:
    outdir: (Optional) The build output directory, defaults to out/Default.
    feature: (Optional) The feature associated with the flag added. If omitted,
      will determine it by building and running `unit_tests
      AboutFlagsHistogramTest.CheckHistograms`. If provided, there's no use also
      providing `outdir`, as nothing needs to be built.
  Example usage:
    generate_flag_enums.py
    generate_flag_enums.py out/Default
    generate_flag_enums.py --feature MyFeatureString
  """

  parser = argparse.ArgumentParser()
  parser.add_argument(
      'outdir',
      nargs='?',
      default='out/Default',
      help='(Optional) The build output directory, defaults to out/Default.')
  parser.add_argument(
      '--feature',
      help="(Optional) The feature associated with the flag added. If omitted, "
      "will determine it by building and running `unit_tests "
      "AboutFlagsHistogramTest.CheckHistograms`. If provided, there's no use "
      "also providing `outdir`, as nothing needs to be built.")
  args = parser.parse_args()

  entries = get_entries_from_feature_string(args.feature) \
    if args.feature else get_entries_from_unit_test(args.outdir)

  if not entries:
    print("No missing enum entries found.")
    return

  xml_dir = path_utils.ScriptDir()
  xml_path = os.path.join(xml_dir, 'enums.xml')

  # Add any missing flag entries to enums.xml.
  with open(xml_path, 'r+') as fd:
    enums_xml = fd.read()
    enums_xml = add_entries_to_xml(enums_xml, entries)
    # Write back the entries to enums.xml.
    fd.seek(0)
    fd.write(enums_xml)

  try:
    # Print any changes.
    completed_process = subprocess.run(
        ['git', 'diff', xml_path],
        capture_output=True,
        encoding='utf-8',
        check=True,
    )
    print(completed_process.stdout)
  except subprocess.CalledProcessError:
    # This may indicate that this is not a git repository. Output a success
    # message instead (as the enums.xml file was updated above).
    print(
        'Successfully updated '
        + xml_path
        + '. Did not display a diff because this does not appear to be a git'
        + 'repository.'
    )


if __name__ == '__main__':
  main()

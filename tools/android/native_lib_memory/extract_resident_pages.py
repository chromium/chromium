#!/usr/bin/env python3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates Json file for native library's resident pages."""

import argparse
import json
import logging
import os
import sys


_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.insert(0, os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils
from devil.android import device_errors


def _CreateArgumentParser():
  parser = argparse.ArgumentParser(
    description='Create JSON file for residency pages')
  parser.add_argument('--device-serial', type=str, required=True)
  parser.add_argument('--on-device-file-path', type=str,
                      help='Path to residency.txt', required=True)
  parser.add_argument('--output-directory', type=str, help='Output directory',
                      required=False)
  return parser


def _ReadFileFromDevice(device_serial, file_path):
  """Reads the file from the device, and returns its content.

  Args:
    device_serial: (str) Device identifier
    file_path: (str) On-device path to the residency file.

  Returns:
    (str or None) The file content.
  """
  content = None
  try:
    device = device_utils.DeviceUtils(device_serial)
    content = device.ReadFile(file_path, True)
  except device_errors.CommandFailedError:
    logging.exception(
        'Possible failure reaching the device or reading the file')

  return content


def ParseResidentPages(resident_pages):
  """Parses and converts the residency data into a list where
  the index corresponds to the page number and the value 1 if resident
  and 0 otherwise.

  |resident_pages| contains a string of resident pages:
      0
      1
      ...
      ...
      N

  Args:
    resident_pages: (str) As returned by ReadFileFromDevice()

  Returns:
    (list) Pages list.
  """
  pages_list = []
  expected = 0
  for page in resident_pages.splitlines():
    while expected < int(page):
      pages_list.append(0)
      expected += 1

    pages_list.append(1)
    expected += 1;
  return pages_list


def _GetResidentPagesJSON(pages_list):
  """Transforms the pages list to JSON object.

  Args:
    pages_list: (list) As returned by ParseResidentPages()

  Returns:
    (JSON object) Pages JSON object.
  """
  json_data = []
  for i in range(len(pages_list)):
    json_data.append({'page_num': i, 'resident': pages_list[i]})
  return json_data


def _WriteJSONToFile(json_data, output_file_path):
  """Dumps JSON data to file.

  Args:
    json_data: (JSON object) Data to be dumped in the file.
    output_file_path: (str) Output file path
  """
  with open(output_file_path, 'w') as f:
    json.dump(json_data, f)


def main():
  parser = _CreateArgumentParser()
  args = parser.parse_args()
  logging.basicConfig(level=logging.INFO)

  content = _ReadFileFromDevice(args.device_serial,
                                args.on_device_file_path)
  if not content:
    logging.error('Error reading file from device')
    return 1

  pages_list = ParseResidentPages(content)
  pages_json = _GetResidentPagesJSON(pages_list)
  _WriteJSONToFile(pages_json, os.path.join(args.output_directory,
                                            'residency.json'))

  return 0


if __name__ == '__main__':
  sys.exit(main())

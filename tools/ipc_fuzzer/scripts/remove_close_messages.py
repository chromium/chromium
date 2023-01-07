# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Removes WidgetHostMsg_Close and alike from testcases. These messages are an
annoyance for corpus distillation. They cause the browser to exit, so no
further messages are processed. On the other hand, WidgetHostMsg_Close is useful
for fuzzing - many found bugs are related to a renderer disappearing. So the
fuzzer should be crafting random WidgetHostMsg_Close messages.
"""

from __future__ import print_function

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile


def create_temp_file():
  temp_file = tempfile.NamedTemporaryFile(delete=False)
  temp_file.close()
  return temp_file.name


def main():
  desc = 'Remove WidgetHostMsg_Close and alike from the testcases.'
  parser = argparse.ArgumentParser(description=desc)
  parser.add_argument(
      '--out-dir',
      dest='out_dir',
      default='out',
      help='ouput directory under src/ directory')
  parser.add_argument(
      '--build-type',
      dest='build_type',
      default='Release',
      help='Debug vs. Release build')
  parser.add_argument('testcase_dir', help='Directory containing testcases')
  parsed = parser.parse_args()

  message_util_binary = 'ipc_message_util'

  script_path = os.path.realpath(__file__)
  ipc_fuzzer_dir = os.path.join(os.path.dirname(script_path), os.pardir)
  src_dir = os.path.abspath(os.path.join(ipc_fuzzer_dir, os.pardir, os.pardir))
  out_dir = os.path.join(src_dir, parsed.out_dir)
  build_dir = os.path.join(out_dir, parsed.build_type)

  message_util_path = os.path.join(build_dir, message_util_binary)
  if not os.path.exists(message_util_path):
    print('ipc_message_util executable not found at ', message_util_path)
    return 1

  filter_command = [
      message_util_path,
      '--invert',
      '--regexp=WidgetHostMsg_Close|WidgetHostMsg_ClosePage_ACK',
      'input',
      'output',
  ]

  testcase_list = os.listdir(parsed.testcase_dir)
  testcase_count = len(testcase_list)
  index = 0
  for testcase in testcase_list:
    index += 1
    print('[%d/%d] Processing %s' % (index, testcase_count, testcase))
    testcase_path = os.path.join(parsed.testcase_dir, testcase)
    filtered_path = create_temp_file()
    filter_command[-2] = testcase_path
    filter_command[-1] = filtered_path
    subprocess.call(filter_command)
    shutil.move(filtered_path, testcase_path)

  return 0


if __name__ == '__main__':
  sys.exit(main())

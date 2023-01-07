# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generational ClusterFuzz fuzzer. It generates IPC messages using
GenerateTraits. Support of GenerateTraits for different types will be gradually
added.
"""

import os
import random
import subprocess
import sys
import utils

FUZZER_NAME_OPTION = '--fuzzer-name=generate'
MAX_IPC_MESSAGES_PER_TESTCASE = 1500


class GenerationalFuzzer:

  def __init__(self):
    self.args = utils.parse_arguments()

    self.ipc_fuzzer_binary = utils.get_fuzzer_application_name()
    self.ipc_fuzzer_binary_path = utils.get_application_path(
        self.ipc_fuzzer_binary)

  def generate_ipcdump_testcase(self):
    ipcdump_testcase_path = (
        utils.random_ipcdump_testcase_path(self.args.output_dir))
    num_ipc_messages = random.randint(1, MAX_IPC_MESSAGES_PER_TESTCASE)
    count_option = '--count=%d' % num_ipc_messages

    cmd = [
        self.ipc_fuzzer_binary_path,
        FUZZER_NAME_OPTION,
        count_option,
        ipcdump_testcase_path,
    ]

    if subprocess.call(cmd):
      sys.exit('%s failed.' % self.ipc_fuzzer_binary)

    utils.create_flags_file(ipcdump_testcase_path)

  def main(self):
    for _ in range(self.args.no_of_files):
      self.generate_ipcdump_testcase()

    return 0


if __name__ == '__main__':
  fuzzer = GenerationalFuzzer()
  sys.exit(fuzzer.main())

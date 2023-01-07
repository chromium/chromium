# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Mutational ClusterFuzz fuzzer. A pre-built corpus of ipcdump files has
to be uploaded to ClusterFuzz along with this script. As chrome is being
developed, the corpus will become out-of-date and needs to be updated.

This fuzzer will pick some ipcdumps from the corpus, concatenate them with
ipc_message_util and mutate the result with ipc_fuzzer_mutate.
"""

import os
import random
import subprocess
import sys
import utils

FUZZER_NAME_OPTION = '--fuzzer-name=mutate'
IPC_MESSAGE_UTIL_APPLICATION = 'ipc_message_util'
IPCDUMP_MERGE_LIMIT = 50


class MutationalFuzzer:

  def __init__(self):
    self.args = utils.parse_arguments()

    self.ipc_message_util_binary = utils.application_name_for_platform(
        IPC_MESSAGE_UTIL_APPLICATION)
    self.ipc_fuzzer_binary = utils.get_fuzzer_application_name()
    self.ipc_message_util_binary_path = utils.get_application_path(
        self.ipc_message_util_binary)
    self.ipc_fuzzer_binary_path = utils.get_application_path(
        self.ipc_fuzzer_binary)

  def set_corpus(self):
    # Corpus should be set per job as a fuzzer-specific environment variable.
    corpus = os.getenv('IPC_CORPUS_DIR', 'default')
    corpus_directory = os.path.join(self.args.input_dir, corpus)
    if not os.path.exists(corpus_directory):
      sys.exit('Corpus directory "%s" not found.' % corpus_directory)

    entries = os.listdir(corpus_directory)
    entries = [i for i in entries if i.endswith(utils.IPCDUMP_EXTENSION)]
    self.corpus = [os.path.join(corpus_directory, entry) for entry in entries]

  def create_mutated_ipcdump_testcase(self):
    ipcdumps = ','.join(random.sample(self.corpus, IPCDUMP_MERGE_LIMIT))
    tmp_ipcdump_testcase = utils.create_temp_file()
    mutated_ipcdump_testcase = (
        utils.random_ipcdump_testcase_path(self.args.output_dir))

    # Concatenate ipcdumps -> tmp_ipcdump.
    cmd = [
        self.ipc_message_util_binary_path,
        ipcdumps,
        tmp_ipcdump_testcase,
    ]
    if subprocess.call(cmd):
      sys.exit('%s failed.' % self.ipc_message_util_binary)

    # Mutate tmp_ipcdump -> mutated_ipcdump.
    cmd = [
        self.ipc_fuzzer_binary_path,
        FUZZER_NAME_OPTION,
        tmp_ipcdump_testcase,
        mutated_ipcdump_testcase,
    ]
    if subprocess.call(cmd):
      sys.exit('%s failed.' % self.ipc_fuzzer_binary)

    utils.create_flags_file(mutated_ipcdump_testcase)
    os.remove(tmp_ipcdump_testcase)

  def main(self):
    self.set_corpus()
    for _ in range(self.args.no_of_files):
      self.create_mutated_ipcdump_testcase()

    return 0


if __name__ == '__main__':
  fuzzer = MutationalFuzzer()
  sys.exit(fuzzer.main())

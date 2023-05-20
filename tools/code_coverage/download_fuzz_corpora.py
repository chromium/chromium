#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Download all the fuzzing corpora associated with all Chromium libfuzzer
targets.

  * Example usage: download_fuzz_corpora.py [DOWNLOAD_DIR]
"""

CORPORA_BUCKET_BASE_URL = "gs://clusterfuzz-corpus/libfuzzer"

import argparse
import coverage_consts
import logging
from multiprocessing import cpu_count, Pool
import os
import subprocess
import sys


def _download_corpus(args):
  target = args[0]
  download_dir = args[1]
  target_path = os.path.join(CORPORA_BUCKET_BASE_URL, target)
  subprocess.run(['gsutil', '-m', 'cp', '-r', target_path, download_dir])


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument('download_dir',
                          type=str,
                          help='Directory into which corpora are downloaded.')
  args = arg_parser.parse_args()
  return args


def Main():
  args = _ParseCommandArguments()

  if not args.download_dir:
    logging.error("No download_dir given")
    exit
  if not os.path.isdir(args.download_dir):
    logging.error("%s does not exist or is not a directory" % args.download_dir)
    exit

  with Pool(cpu_count()) as p:
    results = p.map(_download_corpus,
                    [(corpus, args.download_dir)
                     for corpus in coverage_consts.FUZZERS_WITH_CORPORA])


if __name__ == '__main__':
  sys.exit(Main())

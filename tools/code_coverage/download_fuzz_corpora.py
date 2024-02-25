#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Download all the fuzzing corpora associated with all Chromium libfuzzer
targets.

Assumes that fuzzer targets are already built and reside in the BUILD_DIR
directory.

  * Example usage: download_fuzz_corpora.py --download-dir [DOWNLOAD_DIR]
    --build-dir [BUILD_DIR]
"""

CORPORA_BUCKET_BASE_URL = "gs://clusterfuzz-libfuzzer-backup/corpus/libfuzzer/"

import argparse
import coverage_consts
import logging
from multiprocessing import cpu_count, Pool
import os
import subprocess
import sys


def _gsutil(cmd):
  subprocess.run(cmd)


def _download_corpus(args):
  target = args[0]
  download_dir = args[1]
  target_folder = os.path.join(download_dir, target)
  subprocess.run(['mkdir', target_folder])
  target_path = os.path.join(CORPORA_BUCKET_BASE_URL, target, "latest.zip")
  gsutil_cmd = ['gsutil', 'cp', target_path, target_folder]
  _gsutil(gsutil_cmd)


def _unzip_corpus(args):
  target = args[0]
  download_dir = args[1]
  target_folder = os.path.join(download_dir, target)
  target_path = os.path.join(download_dir, target, "latest.zip")
  subprocess.run(['unzip', "latest.zip"], cwd=target_folder)
  subprocess.run(['rm', 'latest.zip'], cwd=target_folder)
  try:
    # Unzipping the corpora often also contains a "regressions" folder, which
    # is a subset of the total corpus, so can be ignored/removed
    subprocess.run(['rm', '-rf', 'regressions'], cwd=target_folder)
  except:
    pass


def unzip_corpora(download_dir, corpora_to_download):
  with Pool(cpu_count()) as p:
    results = p.map(_unzip_corpus, [(corpus, args.download_dir)
                                    for corpus in corpora_to_download])


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument('--download-dir',
                          type=str,
                          required=True,
                          help='Directory into which corpora are downloaded.')
  arg_parser.add_argument('--build-dir',
                          required=True,
                          type=str,
                          help='Directory where fuzzers were built.')
  args = arg_parser.parse_args()
  return args


def Main():
  args = _ParseCommandArguments()
  exit

  if not args.download_dir:
    logging.error("No download_dir given")
    exit
  if not os.path.isdir(args.download_dir):
    logging.error("%s does not exist or is not a directory" % args.download_dir)
    exit
  if not args.build_dir:
    logging.error("No build_dir given")
    exit
  if not os.path.isdir(args.build_dir):
    logging.error("%s does not exist or is not a directory" % args.build_dir)
    exit

  corpora_to_download = []
  for target in os.listdir(args.build_dir):
    if target.endswith('_fuzzer'):
      corpora_to_download.append(target)

  print("Corpora to download: " + str(corpora_to_download))

  with Pool(cpu_count()) as p:
    results = p.map(_download_corpus, [(corpus, args.download_dir)
                                       for corpus in corpora_to_download])
  with Pool(cpu_count()) as p:
    results = p.map(_unzip_corpus, [(corpus, args.download_dir)
                                    for corpus in corpora_to_download])


if __name__ == '__main__':
  sys.exit(Main())

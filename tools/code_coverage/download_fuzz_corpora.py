#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Download the fuzzing corpora associated with Chromium/V8 fuzz test.

For libfuzzer and centipede, assumes that fuzzer targets are already built
and reside in the BUILD_DIR directory. For fuzzilli download corpora from
gs bucket based on arch.

  * Example usage: download_fuzz_corpora.py --download-dir [DOWNLOAD_DIR]
    --build-dir [BUILD_DIR] --corpora-type [CORPORA_TYPE]
"""

LIBFUZZER_CORPORA_TYPE = 'libfuzzer'
CENTIPEDE_CORPORA_TYPE = 'centipede'
FUZZILLI_CORPORA_TYPE = 'fuzzilli'
ALL_CORPORA_TYPES = [
    LIBFUZZER_CORPORA_TYPE, CENTIPEDE_CORPORA_TYPE, FUZZILLI_CORPORA_TYPE
]
CORPORA_BUCKET_BASE_URL_BY_TYPE = {
    LIBFUZZER_CORPORA_TYPE:
    'gs://clusterfuzz-libfuzzer-backup/corpus/libfuzzer/',
    CENTIPEDE_CORPORA_TYPE:
    'gs://clusterfuzz-centipede-backup/corpus/centipede/',
    FUZZILLI_CORPORA_TYPE: 'gs://autozilli/',
}

import argparse
import coverage_consts
import logging
from multiprocessing import cpu_count, Pool
import os
import re
import subprocess
import sys


def _gsutil(cmd, cwd):
  subprocess.run(cmd, cwd=cwd)


def _get_fuzzilli_corpora(arch):
  output = subprocess.check_output(
      ['gsutil', 'ls',
       CORPORA_BUCKET_BASE_URL_BY_TYPE[FUZZILLI_CORPORA_TYPE]]).decode('utf-8')
  regex = {
      'x64': 'autozilli-[0-9]+\.tgz',
      'x86': 'autozilli-x86-[0-9]+\.tgz',
      'arm64': 'autozilli-arm64-[0-9]+\.tgz',
  }[arch]
  return re.findall(regex, output)


def _download_corpus(args):
  target = args[0]
  download_dir = args[1]
  corpora_type = args[2]
  url = CORPORA_BUCKET_BASE_URL_BY_TYPE[corpora_type]
  if corpora_type == FUZZILLI_CORPORA_TYPE:
    # For a corpora file autozilli-1.tgz, it will be downloaded to
    # [DOWNLOAD_DIR]/autozilli-1/autozilli-1.tgz
    corpus_dir, _ = os.path.splitext(target)
    corpus_url = os.path.join(url, target)
  else:
    corpus_dir = target
    corpus_url = os.path.join(url, target, 'latest.zip')

  subprocess.run(['mkdir', corpus_dir], cwd=download_dir)
  cmd = ['gsutil', 'cp', corpus_url, corpus_dir]
  _gsutil(cmd, download_dir)


def _unzip_corpus(args):
  target = args[0]
  download_dir = args[1]
  target_folder = os.path.join(download_dir, target)
  subprocess.run(['unzip', 'latest.zip'], cwd=target_folder)
  subprocess.run(['rm', 'latest.zip'], cwd=target_folder)
  try:
    # Unzipping the corpora often also contains a "regressions" folder, which
    # is a subset of the total corpus, so can be ignored/removed
    subprocess.run(['rm', '-rf', 'regressions'], cwd=target_folder)
  except:
    pass


def _unzip_fuzzilli_corpus(args):
  corpus = args[0]
  download_dir = args[1]
  base, _ = os.path.splitext(corpus)
  corpus_dir = os.path.join(download_dir, base)
  subprocess.run(['tar', 'xzvf', corpus], cwd=corpus_dir)
  subprocess.run(['rm', corpus], cwd=corpus_dir)


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
  arg_parser.add_argument('--corpora-type',
                          choices=ALL_CORPORA_TYPES,
                          default=LIBFUZZER_CORPORA_TYPE,
                          help='The type of corpora to download.')
  arg_parser.add_argument(
      '--arch',
      choices=['x64', 'x86', 'arm64'],
      default='x64',
      help='The cpu architecture of the target. Fuzzilli only.')
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

  if args.corpora_type == FUZZILLI_CORPORA_TYPE:
    corpora_to_download = _get_fuzzilli_corpora(args.arch)
  else:
    corpora_to_download = []
    for target in os.listdir(args.build_dir):
      if target.endswith('_fuzzer'):
        corpora_to_download.append(target)

  print("Corpora to download: " + str(corpora_to_download))

  with Pool(cpu_count()) as p:
    results = p.map(_download_corpus,
                    [(corpus, args.download_dir, args.corpora_type)
                     for corpus in corpora_to_download])
  unzip_func = (_unzip_fuzzilli_corpus if args.corpora_type
                == FUZZILLI_CORPORA_TYPE else _unzip_corpus)
  with Pool(cpu_count()) as p:
    results = p.map(unzip_func, [(corpus, args.download_dir)
                                 for corpus in corpora_to_download])


if __name__ == '__main__':
  sys.exit(Main())

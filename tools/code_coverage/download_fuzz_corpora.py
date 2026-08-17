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
  LIBFUZZER_CORPORA_TYPE,
  CENTIPEDE_CORPORA_TYPE,
  FUZZILLI_CORPORA_TYPE,
]
CORPORA_BUCKET_BASE_URL_BY_TYPE = {
  LIBFUZZER_CORPORA_TYPE: 'gs://clusterfuzz-libfuzzer-backup/corpus/libfuzzer/',
  CENTIPEDE_CORPORA_TYPE: 'gs://clusterfuzz-centipede-backup/corpus/centipede/',
  FUZZILLI_CORPORA_TYPE: 'gs://autozilli/',
}

import argparse
import logging
from multiprocessing import cpu_count, Pool
import os
import re
import shutil
import subprocess
import sys
import zipfile

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_ROOT = os.path.abspath(os.path.join(_SCRIPT_DIR, '..', '..'))
_GSUTIL_PY = os.path.join(_SRC_ROOT, 'third_party', 'depot_tools', 'gsutil.py')


def _gsutil(cmd, cwd):
  full_cmd = [sys.executable, _GSUTIL_PY] + cmd
  subprocess.run(full_cmd, cwd=cwd, check=True)


def _get_fuzzilli_corpora(arch):
  cmd = [
    sys.executable,
    _GSUTIL_PY,
    'ls',
    CORPORA_BUCKET_BASE_URL_BY_TYPE[FUZZILLI_CORPORA_TYPE],
  ]
  output = subprocess.check_output(cmd).decode('utf-8')
  regex = {
    'x64': r'autozilli-[0-9]+\.tgz',
    'x86': r'autozilli-x86-[0-9]+\.tgz',
    'arm64': r'autozilli-arm64-[0-9]+\.tgz',
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

  os.makedirs(os.path.join(download_dir, corpus_dir), exist_ok=True)
  cmd = ['cp', corpus_url, corpus_dir]
  _gsutil(cmd, download_dir)


def _unzip_corpus(args):
  target = args[0]
  download_dir = args[1]
  target_folder = os.path.join(download_dir, target)
  zip_path = os.path.join(target_folder, 'latest.zip')
  with zipfile.ZipFile(zip_path, 'r') as zip_ref:
    zip_ref.extractall(target_folder)
  os.remove(zip_path)

  # Unzipping the corpora often also contains a "regressions" folder, which
  # is a subset of the total corpus, so can be ignored/removed
  regressions_dir = os.path.join(target_folder, 'regressions')
  shutil.rmtree(regressions_dir, ignore_errors=True)


def _unzip_fuzzilli_corpus(args):
  corpus = args[0]
  download_dir = args[1]
  base, _ = os.path.splitext(corpus)
  corpus_dir = os.path.join(download_dir, base)
  archive_path = os.path.join(corpus_dir, corpus)
  shutil.unpack_archive(archive_path, corpus_dir)
  os.remove(archive_path)


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument(
    '--download-dir',
    type=str,
    required=True,
    help='Directory into which corpora are downloaded.',
  )
  arg_parser.add_argument(
    '--build-dir',
    required=True,
    type=str,
    help='Directory where fuzzers were built.',
  )
  arg_parser.add_argument(
    '--corpora-type',
    choices=ALL_CORPORA_TYPES,
    default=LIBFUZZER_CORPORA_TYPE,
    help='The type of corpora to download.',
  )
  arg_parser.add_argument(
    '--arch',
    choices=['x64', 'x86', 'arm64'],
    default='x64',
    help='The cpu architecture of the target. Fuzzilli only.',
  )
  args = arg_parser.parse_args()
  return args


def Main():
  args = _ParseCommandArguments()

  if not os.path.isdir(args.download_dir):
    logging.error("%s does not exist or is not a directory" % args.download_dir)
    return 1
  if not os.path.isdir(args.build_dir):
    logging.error("%s does not exist or is not a directory" % args.build_dir)
    return 1

  if args.corpora_type == FUZZILLI_CORPORA_TYPE:
    corpora_to_download = _get_fuzzilli_corpora(args.arch)
  else:
    corpora_to_download = set()
    for target in os.listdir(args.build_dir):
      if target.endswith(('_fuzzer', '_fuzzer.exe')):
        target_name = target.removesuffix('.exe')
        corpora_to_download.add(target_name)

  print("Corpora to download: " + str(corpora_to_download))

  with Pool(cpu_count()) as p:
    results = p.map(
      _download_corpus,
      [
        (corpus, args.download_dir, args.corpora_type)
        for corpus in corpora_to_download
      ],
    )
  unzip_func = (
    _unzip_fuzzilli_corpus
    if args.corpora_type == FUZZILLI_CORPORA_TYPE
    else _unzip_corpus
  )
  with Pool(cpu_count()) as p:
    results = p.map(
      unzip_func,
      [(corpus, args.download_dir) for corpus in corpora_to_download],
    )


if __name__ == '__main__':
  sys.exit(Main())

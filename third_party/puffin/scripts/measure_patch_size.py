#!/usr/bin/env python3
#
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""A tool for running diffing tools and measuring patch sizes."""

import argparse
import logging
import os
import subprocess
import sys
import tempfile


class Error(Exception):
  """Puffin general processing error."""


def ParseArguments(argv):
  """Parses and Validates command line arguments.

  Args:
    argv: command line arguments to parse.

  Returns:
    The arguments list.
  """
  parser = argparse.ArgumentParser()

  parser.add_argument('--src-corpus', metavar='DIR',
                      help='The source corpus directory with compressed files.')
  parser.add_argument('--tgt-corpus', metavar='DIR',
                      help='The target corpus directory with compressed files.')
  parser.add_argument('--debug', action='store_true',
                      help='Turns on verbosity.')

  # Parse command-line arguments.
  args = parser.parse_args(argv)

  for corpus in (args.src_corpus, args.tgt_corpus):
    if not corpus or not os.path.isdir(corpus):
      raise Error('Corpus directory {} is non-existent or inaccesible'
                  .format(corpus))
  return args


def main(argv):
  """The main function."""
  args = ParseArguments(argv[1:])

  if args.debug:
    logging.getLogger().setLevel(logging.DEBUG)

  # Construct list of appropriate files.
  src_files = list(filter(os.path.isfile,
                          [os.path.join(args.src_corpus, f)
                           for f in os.listdir(args.src_corpus)]))
  tgt_files = list(filter(os.path.isfile,
                          [os.path.join(args.tgt_corpus, f)
                           for f in os.listdir(args.tgt_corpus)]))

  # Check if all files in src_files have a target file in tgt_files.
  files_mismatch = (set(map(os.path.basename, src_files)) -
                    set(map(os.path.basename, tgt_files)))
  if files_mismatch:
    raise Error('Target files {} do not exist in corpus: {}'
                .format(files_mismatch, args.tgt_corpus))

  for src in src_files:
    with tempfile.NamedTemporaryFile() as puffdiff_patch, \
         tempfile.NamedTemporaryFile() as bsdiff_patch:

      tgt = os.path.join(args.tgt_corpus, os.path.basename(src))

      operation = 'puffdiff'
      cmd = ['puffin',
             '--operation={}'.format(operation),
             '--src_file={}'.format(src),
             '--dst_file={}'.format(tgt),
             '--patch_file={}'.format(puffdiff_patch.name)]
      # Running the puffdiff operation
      if subprocess.call(cmd) != 0:
        raise Error('Puffin failed to do {} command: {}'
                    .format(operation, cmd))

      operation = 'bsdiff'
      cmd = ['bsdiff', '--type', 'bz2', src, tgt, bsdiff_patch.name]
      # Running the bsdiff operation
      if subprocess.call(cmd) != 0:
        raise Error('Failed to do {} command: {}'
                    .format(operation, cmd))

      logging.debug('%s(%d -> %d) : bsdiff(%d), puffdiff(%d)',
                    os.path.basename(src),
                    os.stat(src).st_size, os.stat(tgt).st_size,
                    os.stat(bsdiff_patch.name).st_size,
                    os.stat(puffdiff_patch.name).st_size)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))

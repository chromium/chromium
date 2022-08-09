#!/usr/bin/env python3
#
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""A tool for running Puffin tests in a corpus of deflate compressed files."""

import argparse
import filecmp
import logging
import os
import subprocess
import sys
import tempfile

_PUFFHUFF = 'puffhuff'
_PUFFDIFF = 'puffdiff'
TESTS = (_PUFFHUFF, _PUFFDIFF)


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

  parser.add_argument('corpus', metavar='CORPUS',
                      help='A corpus directory containing compressed files')
  parser.add_argument('-d', '--disabled_tests', default=(), metavar='',
                      nargs='*',
                      help=('Space separated list of tests to disable. '
                            'Allowed options include: ' + ', '.join(TESTS)),
                      choices=TESTS)
  parser.add_argument('--cache_size', type=int, metavar='SIZE',
                      help='The size (in bytes) of the cache for puffpatch '
                      'operations.')
  parser.add_argument('--debug', action='store_true',
                      help='Turns on verbosity.')

  # Parse command-line arguments.
  args = parser.parse_args(argv)

  if not os.path.isdir(args.corpus):
    raise Error('Corpus directory {} is non-existent or inaccesible'
                .format(args.corpus))
  return args


def main(argv):
  """The main function."""
  args = ParseArguments(argv[1:])

  if args.debug:
    logging.getLogger().setLevel(logging.DEBUG)

  # Construct list of appropriate files.
  files = list(filter(os.path.isfile, [os.path.join(args.corpus, f)
                                       for f in os.listdir(args.corpus)]))

  # For each file in corpus run puffhuff.
  if _PUFFHUFF not in args.disabled_tests:
    for src in files:
      with tempfile.NamedTemporaryFile() as tgt_file:

        operation = 'puffhuff'
        logging.debug('Running %s on %s', operation, src)
        cmd = ['puffin',
               '--operation={}'.format(operation),
               '--src_file={}'.format(src),
               '--dst_file={}'.format(tgt_file.name)]
        if subprocess.call(cmd) != 0:
          raise Error('Puffin failed to do {} command: {}'
                      .format(operation, cmd))

        if not filecmp.cmp(src, tgt_file.name):
          raise Error('The generated file {} is not equivalent to the original '
                      'file {} after {} operation.'
                      .format(tgt_file.name, src, operation))

  if _PUFFDIFF not in args.disabled_tests:
    # Run puffdiff and puffpatch for each pairs of files in the corpus.
    for src in files:
      for tgt in files:
        with tempfile.NamedTemporaryFile() as patch, \
             tempfile.NamedTemporaryFile() as new_tgt:

          operation = 'puffdiff'
          logging.debug('Running %s on %s (%d) and %s (%d)',
                        operation,
                        os.path.basename(src), os.stat(src).st_size,
                        os.path.basename(tgt), os.stat(tgt).st_size)
          cmd = ['puffin',
                 '--operation={}'.format(operation),
                 '--src_file={}'.format(src),
                 '--dst_file={}'.format(tgt),
                 '--patch_file={}'.format(patch.name)]

          # Running the puffdiff operation
          if subprocess.call(cmd) != 0:
            raise Error('Puffin failed to do {} command: {}'
                        .format(operation, cmd))

          logging.debug('Patch size is: %d', os.stat(patch.name).st_size)

          operation = 'puffpatch'
          logging.debug('Running %s on src file %s and patch %s',
                        operation, os.path.basename(src), patch.name)
          cmd = ['puffin',
                 '--operation={}'.format(operation),
                 '--src_file={}'.format(src),
                 '--dst_file={}'.format(new_tgt.name),
                 '--patch_file={}'.format(patch.name)]
          if args.cache_size:
            cmd += ['--cache_size={}'.format(args.cache_size)]

          # Running the puffpatch operation
          if subprocess.call(cmd) != 0:
            raise Error('Puffin failed to do {} command: {}'
                        .format(operation, cmd))

          if not filecmp.cmp(tgt, new_tgt.name):
            raise Error('The generated file {} is not equivalent to the '
                        'original file {} after puffpatch operation.'
                        .format(new_tgt.name, tgt))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))

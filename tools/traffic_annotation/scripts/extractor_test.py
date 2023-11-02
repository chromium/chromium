#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for extractor.py.
"""

from __future__ import print_function

import argparse
import os
import unittest
import re
import subprocess

from glob import glob
from pathlib import Path
from typing import Tuple

# Path to the directory where this script is.
SCRIPT_DIR = Path(__file__).resolve().parent


def run_extractor(file_path: Path) -> Tuple[bytes, bytes]:
  script_path = Path('../extractor.py')
  cmd_line = ('python3', str(script_path), '--no-filter', file_path)
  return subprocess.Popen(
      cmd_line, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def get_expected_files(source_file: Path) -> Path:
  stdout_file = (source_file.with_name('%s-stdout' %
                                       source_file.stem).with_suffix('.txt'))
  stderr_file = (source_file.with_name('%s-stderr' %
                                       source_file.stem).with_suffix('.txt'))
  return (stdout_file, stderr_file)


def dos2unix(body: str):
  """Converts CRLF to LF."""
  return body.replace('\r\n', '\n')


def remove_tracebacks(body: str):
  """Removes python tracebacks from the string."""
  regex = re.compile(
      r'''
       # A line that says "Traceback (...):"
       ^Traceback[^\n]*:\n
       # Followed by lines that begin with whitespace.
       ((\s.*)?\n)*''',
      re.MULTILINE | re.VERBOSE)
  return re.sub(regex, '', body)


class ExtractorTest(unittest.TestCase):
  def testExtractor(self):
    files = list(Path(f) for f in glob('*.cc') + glob('*.java'))
    for source_file in files:
      if source_file.stem.startswith('test_'):
        continue

      print("Running test on %s..." % source_file)
      (stdout_file, stderr_file) = get_expected_files(source_file)
      expected_stdout = dos2unix(stdout_file.read_text())
      expected_stderr = dos2unix(stderr_file.read_text())

      proc = run_extractor(source_file)
      (stdout, stderr) = (dos2unix(b.decode()) for b in proc.communicate())

      self.assertEqual(expected_stderr, remove_tracebacks(stderr))
      expected_returncode = 2 if expected_stderr else 0
      self.assertEqual(expected_returncode, proc.returncode)
      self.assertEqual(expected_stdout, stdout)


def generate_expected_files():
  files = list(Path(f)
               for f in glob('*.java'))  # glob('*.cc') + glob('*.java'))
  for source_file in files:
    proc = run_extractor(source_file)
    (stdout, stderr) = (b.decode() for b in proc.communicate())

    (stdout_file, stderr_file) = get_expected_files(source_file)
    stdout_file.write_text(stdout)
    stderr_file.write_text(stderr)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description="Network Traffic Annotation Extractor Unit Tests")
  parser.add_argument(
      '--generate-expected-files', action='store_true',
      help='Generate "-stdout.txt" and "-stderr.txt" for file in test_data')
  args = parser.parse_args()

  # Set directory for both test and gen command to the test_data folder.
  os.chdir(Path(__file__).resolve().parent / 'test_data')

  # XXX
  # # Run the extractor script with --generate-compdb to ensure the
  # # compile_commands.json file exists in the default output directory.
  # proc = run_extractor(os.devnull, '--generate-compdb')
  # proc.communicate()  # Wait until extractor finishes running.

  if args.generate_expected_files:
    generate_expected_files()
  else:
    unittest.main()

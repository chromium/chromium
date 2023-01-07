# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration tests."""

import argparse
import builtins
from collections import defaultdict
import difflib
from functools import wraps
import glob
import json
import os
import unittest
import sys
import tempfile
import pprint
from typing import List, Dict

import disable
import resultdb


def cmd_record(args: argparse.Namespace):
  record_testcase(args.name, ['./disable'] + args.args)


def record_testcase(name: str, testcase_args: List[str]):
  # While running the test, point CANNED_RESPONSE_FILE to a temporary that we
  # can recover data from afterwards.
  fd, temp_canned_response_file = tempfile.mkstemp()
  os.fdopen(fd).close()
  resultdb.CANNED_RESPONSE_FILE = temp_canned_response_file

  original_open = builtins.open
  builtins.open = opener(original_open)

  try:
    disable.main(testcase_args)
    # TODO: We probably want to test failure cases as well. We can add an
    # "exception" field to the testcase JSON and test that the same exception is
    # raised.
  finally:
    builtins.open = original_open
    with open(temp_canned_response_file) as f:
      recorded_requests = f.read()
    os.remove(temp_canned_response_file)

  testcase = {
      'args': testcase_args,
      'requests': recorded_requests,
      'read_data': TrackingFile.read_data,
      'written_data': TrackingFile.written_data,
  }

  print(f'Recorded testcase {name}.\nDiff from this testcase is:\n')

  print_diffs(TrackingFile.read_data, TrackingFile.written_data)

  with open(os.path.join('tests', f'{name}.json'), 'w') as f:
    json.dump(testcase, f, indent=2)

  TrackingFile.read_data.clear()
  TrackingFile.written_data.clear()


def print_diffs(read_data: Dict[str, str], written_data: Dict[str, str]):
  def lines(s: str) -> List[str]:
    return [line + '\n' for line in s.split('\n')]

  for filename in read_data:
    if filename in written_data:
      before = lines(read_data[filename])
      after = lines(written_data[filename])

      sys.stdout.writelines(
          difflib.unified_diff(before,
                               after,
                               fromfile=f'a/{filename}',
                               tofile=f'b/{filename}'))


def opener(old_open):
  @wraps(old_open)
  def tracking_open(path, mode='r', **kwargs):
    if os.path.abspath(path).startswith(disable.SRC_ROOT):
      return TrackingFile(old_open, path, mode, **kwargs)
    return old_open(path, mode, **kwargs)

  return tracking_open


class TrackingFile:
  """A file-like class that records what data was read/written."""

  read_data = {}
  written_data = defaultdict(str)

  def __init__(self, old_open, path, mode, **kwargs):
    self.path = path

    if mode != 'w':
      self.file = old_open(path, mode, **kwargs)
    else:
      self.file = None

  def read(self, n_bytes=-1):
    # It's easier to stash all the results if we only deal with the case where
    # all the data is read at once. Right now we can get away with this as the
    # tool only does this, but if that changes we'll need to support it here.
    assert n_bytes == -1
    data = self.file.read(n_bytes)

    TrackingFile.read_data[src_root_relative(self.path)] = data

    return data

  def write(self, data):
    # Don't actually write the data, since we're just recording a testcase.
    TrackingFile.written_data[src_root_relative(self.path)] += data

  def __enter__(self):
    return self

  def __exit__(self, e_type, e_val, e_tb):
    if self.file is not None:
      self.file.close()
    self.file = None


def src_root_relative(path: str) -> str:
  if os.path.abspath(path).startswith(disable.SRC_ROOT):
    return os.path.relpath(path, disable.SRC_ROOT)
  return path


class IntegrationTest(unittest.TestCase):
  """This class represents a data-driven integration test.

  Given a list of arguments to pass to the test disabler, a set of ResultDB
  requests and responses to replay, and the data read/written to the filesystem,
  run the test disabler in a hermetic test environment and check that the output
  is the same.
  """

  def __init__(self, name, args, requests, read_data, written_data):
    unittest.TestCase.__init__(self, methodName='test_one_testcase')

    self.name = name
    self.args = args
    self.requests = requests
    self.read_data = read_data
    self.written_data = written_data

  def test_one_testcase(self):
    fd, temp_canned_response_file = tempfile.mkstemp(text=True)
    os.fdopen(fd).close()
    with open(temp_canned_response_file, 'w') as f:
      f.write(self.requests)
    resultdb.CANNED_RESPONSE_FILE = temp_canned_response_file

    TrackingFile.read_data.clear()
    TrackingFile.written_data.clear()

    with tempfile.TemporaryDirectory() as temp_dir:
      disable.SRC_ROOT = temp_dir
      for filename, contents in self.read_data.items():
        in_temp = os.path.join(temp_dir, filename)
        os.makedirs(os.path.dirname(in_temp))
        with open(in_temp, 'w') as f:
          f.write(contents)

      original_open = builtins.open
      builtins.open = opener(original_open)

      try:
        disable.main(self.args)
      finally:
        os.remove(temp_canned_response_file)
        builtins.open = original_open

      for path, data in TrackingFile.written_data.items():
        if path == temp_canned_response_file:
          continue

        relpath = src_root_relative(path)

        self.assertIn(relpath, self.written_data)
        self.assertEqual(data, self.written_data[relpath])

  def shortDescription(self):
    return self.name


def cmd_show(args: argparse.Namespace):
  try:
    with open(os.path.join('tests', f'{args.name}.json'), 'r') as f:
      testcase = json.load(f)
  except FileNotFoundError:
    print(f"No such testcase '{args.name}'", file=sys.stderr)
    sys.exit(1)

  command_line = ' '.join(testcase['args'])
  print(f'Testcase {args.name}, invokes disabler with:\n{command_line}\n\n')

  # Pretty-print ResultDB RPC requests and corresponding responses.
  requests = json.loads(testcase['requests'])
  if len(requests) != 0:
    print(f'Makes {len(requests)} request(s) to ResultDB:')

    for request, response in requests.items():
      n = request.index('/')
      name = request[:n]
      payload = json.loads(request[n + 1:])

      print(f'\n{name}')
      pprint.pprint(payload)

      print('->')
      pprint.pprint(json.loads(response))

    print('\n')

  # List all files read.
  read_data = testcase['read_data']
  if len(read_data) > 0:
    print(f'Reads {len(read_data)} file(s):')
    print('\n'.join(read_data))
    print('\n')

  # Show diff between read and written for all written files.
  written_data = testcase['written_data']
  if len(written_data) > 0:
    print('Produces the following diffs:')
    print_diffs(read_data, written_data)


def all_testcase_jsons():
  for testcase in glob.glob('tests/*.json'):
    with open(testcase, 'r') as f:
      yield os.path.basename(testcase)[:-5], json.load(f)


def cmd_run(_args: argparse.Namespace):
  testcases = []
  for name, testcase_json in all_testcase_jsons():
    testcases.append(
        IntegrationTest(
            name,
            testcase_json['args'],
            testcase_json['requests'],
            testcase_json['read_data'],
            testcase_json['written_data'],
        ))

  test_runner = unittest.TextTestRunner()
  test_runner.run(unittest.TestSuite(testcases))


def cmd_rerecord(_args: argparse.Namespace):
  for name, testcase_json in all_testcase_jsons():
    record_testcase(name, testcase_json['args'])
    print('')


def main():
  parser = argparse.ArgumentParser(
      description='Record / replay integration tests.', )

  subparsers = parser.add_subparsers()

  record_parser = subparsers.add_parser('record', help='Record a testcase')
  record_parser.add_argument('name',
                             type=str,
                             help='The name to give the testcase')
  record_parser.add_argument(
      'args',
      type=str,
      nargs='+',
      help='The arguments to use for running the testcase.')
  record_parser.set_defaults(func=cmd_record)

  run_parser = subparsers.add_parser('run', help='Run all testcases')
  run_parser.set_defaults(func=cmd_run)

  show_parser = subparsers.add_parser('show', help='Describe a testcase')
  show_parser.add_argument('name', type=str, help='The testcase to describe')
  show_parser.set_defaults(func=cmd_show)

  rerecord_parser = subparsers.add_parser(
      'rerecord', help='Re-record all existing testcases')
  rerecord_parser.set_defaults(func=cmd_rerecord)

  args = parser.parse_args()
  args.func(args)


if __name__ == '__main__':
  main()

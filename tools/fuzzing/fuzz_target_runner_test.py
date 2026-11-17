#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for fuzz_target_runner.py and engines.py."""

import argparse
import contextlib
import io
import os
import pathlib
import unittest
from unittest import mock

import engines
import fuzz_target_runner as runner

_ARGV = [
  '--engine=libfuzzer',
  '--build-dir=out/fuzz',
  '--target=foo_fuzzer',
  '--duration=15m',
  '--output-dir=o',
]


class DurationTest(unittest.TestCase):
  def test_parse(self):
    self.assertEqual(runner.parse_duration('900'), 900)
    self.assertEqual(runner.parse_duration('900s'), 900)
    self.assertEqual(runner.parse_duration('15m'), 900)
    self.assertEqual(runner.parse_duration('1h'), 3600)
    self.assertEqual(runner.parse_duration('1h30m'), 5400)
    for bad in ('', 'abc', '15x', '1h 2m', '-5', '0', '0s', '0h0m', '\u00b2'):
      with self.assertRaises(argparse.ArgumentTypeError):
        runner.parse_duration(bad)


class SessionFromArgvTest(unittest.TestCase):
  def test_minimal(self):
    session = runner.session_from_argv(_ARGV)
    self.assertIsInstance(session.adapter, engines.LibFuzzerAdapter)
    self.assertEqual(
      session.target,
      engines.Target(pathlib.Path(os.path.abspath('out/fuzz')), 'foo_fuzzer'),
    )
    self.assertEqual(
      session.options, engines.EngineOptions(duration_s=900, seed=None)
    )
    self.assertEqual(session.layout.root, pathlib.Path(os.path.abspath('o')))

  def test_seed(self):
    session = runner.session_from_argv(_ARGV + ['--seed=7'])
    self.assertEqual(session.options.seed, 7)

  def test_strips_whitespace(self):
    session = runner.session_from_argv(
      [
        '--engine=libfuzzer',
        '--build-dir= out/fuzz ',
        '--target= foo_fuzzer ',
        '--duration=15m',
        '--output-dir= o ',
      ]
    )
    expected = runner.session_from_argv(_ARGV)
    self.assertEqual(session.target, expected.target)
    self.assertEqual(session.layout, expected.layout)

  def test_empty_flags(self):
    for flag in ('--build-dir', '--target', '--output-dir'):
      for value in ('', ' '):
        argv = {
          '--engine': 'libfuzzer',
          '--build-dir': 'out/fuzz',
          '--target': 'foo_fuzzer',
          '--duration': '15m',
          '--output-dir': 'o',
          flag: value,
        }
        with self.subTest(flag=flag, value=value):
          with self.assertRaises(SystemExit) as e:
            with contextlib.redirect_stderr(io.StringIO()) as err:
              runner.session_from_argv([f'{k}={v}' for k, v in argv.items()])
          self.assertEqual(e.exception.code, runner.ExitCode.BAD_SPEC)
          self.assertIn('%s: must not be empty' % flag, err.getvalue())


class GetAdapterTest(unittest.TestCase):
  def test_get_adapter(self):
    for name in engines.ENGINES:
      with self.subTest(name=name):
        self.assertEqual(engines.get_adapter(name).name, name)


class MainTest(unittest.TestCase):
  def test_setup_error_becomes_exit_code(self):
    error = runner.SetupError('boom', runner.ExitCode.MISSING_BINARY)
    with mock.patch.object(runner.Session, 'run', side_effect=error):
      with contextlib.redirect_stderr(io.StringIO()) as err:
        exit_code = runner.main(_ARGV)
    self.assertEqual(exit_code, runner.ExitCode.MISSING_BINARY)
    self.assertIn('boom', err.getvalue())


if __name__ == '__main__':
  unittest.main()

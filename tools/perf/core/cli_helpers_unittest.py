# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import unittest
from unittest import mock

import six

from core import cli_helpers
from telemetry import decorators

BUILTIN_MODULE = '__builtin__' if six.PY2 else 'builtins'


class CLIHelpersTest(unittest.TestCase):
  def testUnsupportedColor(self):
    with self.assertRaises(AssertionError):
      cli_helpers.Colored('message', 'pink')

  @mock.patch(BUILTIN_MODULE + '.print')
  def testPrintsInfo(self, print_mock):
    cli_helpers.Info('foo {sval} {ival}', sval='s', ival=42)
    print_mock.assert_called_once_with('foo s 42')

  @mock.patch(BUILTIN_MODULE + '.print')
  def testPrintsComment(self, print_mock):
    cli_helpers.Comment('foo')
    print_mock.assert_called_once_with('\033[93mfoo\033[0m')

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('sys.exit')
  def testFatal(self, sys_exit_mock, print_mock):
    cli_helpers.Fatal('foo')
    print_mock.assert_called_once_with('\033[91mfoo\033[0m')
    sys_exit_mock.assert_called_once()

  @mock.patch(BUILTIN_MODULE + '.print')
  def testPrintsError(self, print_mock):
    cli_helpers.Error('foo')
    print_mock.assert_called_once_with('\033[91mfoo\033[0m')

  @mock.patch(BUILTIN_MODULE + '.print')
  def testPrintsStep(self, print_mock):
    long_step_name = 'foobar' * 15
    cli_helpers.Step(long_step_name)
    self.assertListEqual(print_mock.call_args_list, [
        mock.call('\033[92m' + ('=' * 90) + '\033[0m'),
        mock.call('\033[92m' + long_step_name + '\033[0m'),
        mock.call('\033[92m' + ('=' * 90) + '\033[0m'),
    ])

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('core.cli_helpers.input')
  # https://crbug.com/938575.
  @decorators.Disabled('chromeos')
  def testAskAgainOnInvalidAnswer(self, input_mock, print_mock):
    input_mock.side_effect = ['foobar', 'y']
    self.assertTrue(cli_helpers.Ask('Ready?'))
    self.assertListEqual(print_mock.mock_calls, [
      mock.call('\033[96mReady? [no/YES] \033[0m', end=' '),
      mock.call('\033[91mPlease respond with "no" or "yes".\033[0m'),
      mock.call('\033[96mReady? [no/YES] \033[0m', end=' ')
    ])

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('core.cli_helpers.input')
  # https://crbug.com/938575.
  @decorators.Disabled('chromeos')
  def testAskWithCustomAnswersAndDefault(self, input_mock, print_mock):
    input_mock.side_effect = ['']
    self.assertFalse(
        cli_helpers.Ask('Ready?', {'foo': True, 'bar': False}, default='bar'))
    print_mock.assert_called_once_with(
        '\033[96mReady? [BAR/foo] \033[0m', end=' ')

  @mock.patch('core.cli_helpers.input')
  # https://crbug.com/938575.
  @decorators.Disabled('chromeos')
  def testAskWithCustomAnswersAndCaps(self, input_mock):
    input_mock.side_effect = ['Foo/Bar/Baz']
    self.assertEqual(
        cli_helpers.Ask('Ready?', ["Foo/Bar/Baz", "other"]),
        'Foo/Bar/Baz')

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('core.cli_helpers.input')
  # https://crbug.com/938575.
  @decorators.Disabled('chromeos')
  def testAskNoDefaultCustomAnswersAsList(self, input_mock, print_mock):
    input_mock.side_effect = ['', 'FoO']
    self.assertEqual(cli_helpers.Ask('Ready?', ['foo', 'bar']), 'foo')
    self.assertListEqual(print_mock.mock_calls, [
      mock.call('\033[96mReady? [foo/bar] \033[0m', end=' '),
      mock.call('\033[91mPlease respond with "bar" or "foo".\033[0m'),
      mock.call('\033[96mReady? [foo/bar] \033[0m', end=' ')
    ])

  def testAskWithInvalidDefaultAnswer(self):
    with self.assertRaises(ValueError):
      cli_helpers.Ask('Ready?', ['foo', 'bar'], 'baz')

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('subprocess.check_call')
  @mock.patch(BUILTIN_MODULE + '.open')
  @mock.patch('datetime.datetime')
  def testCheckLog(
      self, dt_mock, open_mock, check_call_mock, print_mock):
    file_mock = mock.Mock()
    open_mock.return_value.__enter__.return_value = file_mock
    dt_mock.now.return_value.strftime.return_value = '_2018_12_10_16_22_11'

    cli_helpers.CheckLog(
        ['command', 'arg with space'], '/tmp/tmpXYZ.tmp', env={'foo': 'bar'})

    check_call_mock.assert_called_once_with(
        ['command', 'arg with space'],
        stdout=file_mock,
        stderr=subprocess.STDOUT,
        shell=False,
        env={'foo': 'bar'})
    open_mock.assert_called_once_with('/tmp/tmpXYZ.tmp', 'w')
    self.assertListEqual(print_mock.mock_calls, [
      mock.call("\033[94mcommand 'arg with space'\033[0m"),
      mock.call('\033[94mLogging stdout & stderr to /tmp/tmpXYZ.tmp\033[0m'),
    ])

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('core.cli_helpers.Error')
  @mock.patch('subprocess.check_call')
  @mock.patch('subprocess.call')
  @mock.patch(BUILTIN_MODULE + '.open')
  def testCheckLogError(
      self, open_mock, call_mock, check_call_mock, error_mock, print_mock):
    del print_mock, open_mock  # Unused.
    check_call_mock.side_effect = [subprocess.CalledProcessError(87, ['cmd'])]

    with self.assertRaises(subprocess.CalledProcessError):
      cli_helpers.CheckLog(['cmd'], '/tmp/tmpXYZ.tmp')

    call_mock.assert_called_once_with(['cat', '/tmp/tmpXYZ.tmp'])
    self.assertListEqual(error_mock.mock_calls, [
      mock.call('=' * 80),
      mock.call('Received non-zero return code. Log content:'),
      mock.call('=' * 80),
      mock.call('=' * 80),
    ])

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('subprocess.check_call')
  def testRun(self, check_call_mock, print_mock):
    check_call_mock.side_effect = [subprocess.CalledProcessError(87, ['cmd'])]
    with self.assertRaises(subprocess.CalledProcessError):
      cli_helpers.Run(['cmd', 'arg with space'], env={'a': 'b'})
    check_call_mock.assert_called_once_with(
        ['cmd', 'arg with space'], env={'a': 'b'})
    print_mock.assert_called_once_with('\033[94mcmd \'arg with space\'\033[0m')

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('subprocess.check_call')
  def testRunOkFail(self, check_call_mock, print_mock):
    del print_mock  # Unused.
    check_call_mock.side_effect = [subprocess.CalledProcessError(87, ['cmd'])]
    cli_helpers.Run(['cmd'], ok_fail=True)

  def testRunWithNonListCommand(self):
    with self.assertRaises(ValueError):
      cli_helpers.Run('cmd with args')

  @mock.patch(BUILTIN_MODULE + '.print')
  @mock.patch('core.cli_helpers.input')
  def testPrompt(self, input_mock, print_mock):
    input_mock.side_effect = ['', '42']
    self.assertEqual(cli_helpers.Prompt(
        'What is the ultimate meaning of life, universe and everything?'), '42')
    self.assertEqual(input_mock.call_count, 2)
    self.assertEqual(print_mock.call_count, 3)



if __name__ == "__main__":
  unittest.main()

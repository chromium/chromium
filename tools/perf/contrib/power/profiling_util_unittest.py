# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest
from unittest import mock

from contrib.power import profiling_util


class MockPopen(object):
  """Helper class for unit tests that mock subprocess.Popen."""

  def __init__(self, return_code, stdout=None, stderr=None):
    self.return_code = return_code
    self._stdout = stdout
    self._stderr = stderr

  def communicate(self):
    return self._stdout, self._stderr


class ProfilingUtilTests(unittest.TestCase):
  def __init__(self, *args, **kwargs):
    super(ProfilingUtilTests, self).__init__(*args, **kwargs)
    # Working directory for testing.
    self._temp_directory = None
    # Placeholder paths used for testing.
    self._trace_path = None
    self._symbols_path = None
    self._profile_file_name = None
    # The directory where the profile is generated.
    self._profile_source_dir = None
    # The directory where the profile should eventually be copied to.
    self._profile_path = None

  def setUp(self):
    self._temp_directory = tempfile.mkdtemp()
    self.addCleanup(self.cleanup)

    self._trace_path = os.path.join(self._temp_directory, 'trace')
    with open(self._trace_path, 'w') as trace_file:
      trace_file.write('placeholder_trace')

    self._symbols_path = os.path.join(self._temp_directory, 'symbols')

    self._profile_file_name = 'pprof'
    # This needs to include 'perf_profile-', since this is what `traceconv`
    # outputs, too.
    self._profile_source_dir = os.path.join(self._temp_directory,
                                            'perf_profile-dir')
    self._profile_path = os.path.join(self._temp_directory,
                                      self._profile_file_name)

    os.environ['PERFETTO_BINARY_PATH'] = 'placeholder_binary_path'

  def cleanup(self):
    shutil.rmtree(self._temp_directory)
    os.environ.pop('PERFETTO_BINARY_PATH', None)

  # *args and **kwargs are needed for the mock to be able to accept extra
  # arguments.
  # pylint: disable=unused-argument
  def writePlaceholderProfile(self, *args, **kwargs):
    os.makedirs(self._profile_source_dir)
    with open(os.path.join(self._profile_source_dir, self._profile_file_name),
              'w') as profile_file:
      profile_file.write('placeholder_pprof')
    # Return output needs to include self._profile_source_dir, since this is
    # what `traceconv` does, too.
    return MockPopen(return_code=0,
                     stdout='Outputting to {}'.format(
                         self._profile_source_dir).encode('utf-8'))

  @mock.patch('contrib.power.profiling_util.logging.warning')
  def testSymbolizeTraceWithoutBinaryPath(self, mock_warning):
    os.environ.pop('PERFETTO_BINARY_PATH', None)
    profiling_util.SymbolizeTrace(self._trace_path)
    mock_warning.assert_called()

  @mock.patch('contrib.power.profiling_util.logging.error')
  @mock.patch('contrib.power.profiling_util.subprocess.Popen')
  def testSymbolizeTraceWithTraceconvError(self, mock_popen, mock_error):
    mock_popen.return_value = MockPopen(
        return_code=-1, stderr='placeholder_error'.encode('utf-8'))
    profiling_util.SymbolizeTrace(self._trace_path)
    mock_error.assert_called()

  @mock.patch('contrib.power.profiling_util.subprocess.Popen')
  def testSymbolizeTraceWithTraceconvSuccess(self, mock_popen):
    mock_popen.return_value = MockPopen(
        return_code=0, stdout='placeholder_symbols'.encode('utf-8'))
    profiling_util.SymbolizeTrace(self._trace_path)
    with open(self._trace_path, 'r') as trace_file:
      self.assertEqual(trace_file.read(),
                       'placeholder_traceplaceholder_symbols')

  @mock.patch('contrib.power.profiling_util.logging.error')
  @mock.patch('contrib.power.profiling_util.subprocess.Popen')
  def testGenerateProfilesWithTraceconvError(self, mock_popen, mock_error):
    mock_popen.return_value = MockPopen(
        return_code=-1, stderr='placeholder_error'.encode('utf-8'))
    profiling_util.GenerateProfiles(self._trace_path)
    mock_error.assert_called()

  @mock.patch('contrib.power.profiling_util.logging.error')
  @mock.patch('contrib.power.profiling_util.subprocess.Popen')
  def testGenerateProfilesWithInvalidTraceconvOutput(self, mock_popen,
                                                     mock_error):
    mock_popen.return_value = MockPopen(
        return_code=0, stdout='placeholder_output'.encode('utf-8'))
    profiling_util.GenerateProfiles(self._trace_path)
    mock_error.assert_called()

  @mock.patch('contrib.power.profiling_util.subprocess.Popen')
  def testGenerateProfilesWithTraceconvSuccess(self, mock_popen):
    mock_popen.side_effect = self.writePlaceholderProfile
    profiling_util.GenerateProfiles(self._trace_path)
    with open(self._profile_path, 'r') as profile_file:
      self.assertEqual(profile_file.read(), 'placeholder_pprof')

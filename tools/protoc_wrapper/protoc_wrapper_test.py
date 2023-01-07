#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for protoc_wrapper."""

from __future__ import print_function

import logging
import sys
import unittest

if sys.version_info.major == 2:
  from StringIO import StringIO
  import mock
else:
  from io import StringIO
  from unittest import mock

import protoc_wrapper


class ProtocWrapperTest(unittest.TestCase):
  @mock.patch('subprocess.call', return_value=0)
  def test_minimal_arguments(self, mock_call):
    protoc_wrapper.main(
        ['--proto-in-dir', './', '--protoc', '/foo/protoc', 'foo.proto'])
    mock_call.assert_called_once_with(
        ['/foo/protoc', '--proto_path', '.', './foo.proto'])

  @mock.patch('subprocess.call', return_value=0)
  def test_kythe_no_out(self, mock_call):
    protoc_wrapper.main([
        '--proto-in-dir', './', '--enable-kythe-annotation', '--protoc',
        '/foo/protoc', 'foo.proto'
    ])
    mock_call.assert_called_once_with(
        ['/foo/protoc', '--proto_path', '.', './foo.proto'])

  @mock.patch('subprocess.call', return_value=0)
  def test_kythe_cpp_out_no_options(self, mock_call):
    protoc_wrapper.main([
        '--proto-in-dir', './', '--enable-kythe-annotation', '--cc-out-dir',
        './bar', '--protoc', '/foo/protoc', 'foo.proto'
    ])
    mock_call.assert_called_once_with([
        '/foo/protoc', '--cpp_out',
        'annotate_headers,annotation_pragma_name=kythe_metadata,annotation_guard_name=KYTHE_IS_RUNNING:./bar',
        '--proto_path', '.', './foo.proto'
    ])

  @mock.patch('subprocess.call', return_value=0)
  def test_kythe_cpp_out_with_options(self, mock_call):
    protoc_wrapper.main([
        '--proto-in-dir', './', '--enable-kythe-annotation', '--cc-options',
        'foo=bar:', '--cc-out-dir', './bar', '--protoc', '/foo/protoc',
        'foo.proto'
    ])
    mock_call.assert_called_once_with([
        '/foo/protoc', '--cpp_out',
        'annotate_headers,annotation_pragma_name=kythe_metadata,annotation_guard_name=KYTHE_IS_RUNNING,foo=bar:./bar',
        '--proto_path', '.', './foo.proto'
    ])

  @mock.patch('subprocess.call', return_value=0)
  def test_kythe_cpp_out_with_options_no_colon(self, mock_call):
    protoc_wrapper.main([
        '--proto-in-dir', './', '--enable-kythe-annotation', '--cc-options',
        'foo=bar', '--cc-out-dir', './bar', '--protoc', '/foo/protoc',
        'foo.proto'
    ])
    mock_call.assert_called_once_with([
        '/foo/protoc', '--cpp_out',
        'annotate_headers,annotation_pragma_name=kythe_metadata,annotation_guard_name=KYTHE_IS_RUNNING,foo=bar:./bar',
        '--proto_path', '.', './foo.proto'
    ])

  @mock.patch('subprocess.call', return_value=0)
  def test_cpp_out_with_options_no_colon(self, mock_call):
    protoc_wrapper.main([
        '--proto-in-dir', './', '--cc-options', 'foo=bar:', '--cc-out-dir',
        './bar', '--protoc', '/foo/protoc', 'foo.proto'
    ])
    mock_call.assert_called_once_with([
        '/foo/protoc', '--cpp_out', 'foo=bar:./bar', '--proto_path', '.',
        './foo.proto'
    ])


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()

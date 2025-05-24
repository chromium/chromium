#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for builder-cache-clobber.py."""

import argparse
import hashlib
import json
import importlib
import sys
import unittest
from unittest import mock
from io import StringIO

import clobber_cache_utils

builder_cache_clobber = importlib.import_module("builder-cache-clobber")


class BuilderCacheClobberTest(unittest.TestCase):

  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_flow(self, mock_confirm_and_trigger_clobber_bots):
    """Tests the main flow with no bot ID or Xcode caches."""
    mock_confirm_and_trigger_clobber_bots.return_value = [{
        'bot_id': 'bot1',
        'dimensions': []
    }]

    args = [
        '--swarming-server',
        'test-server',
        '--builder',
        'test-builder',
        '--bucket',
        'test-bucket',
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 0)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    # Verify that confirm_and_trigger_clobber_bots was called with the correct arguments.
    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, False, None)
    # The main function of builder-cache-clobber.py *shouldn't* produce any
    # output in the basic success case.  All output is handled within
    # confirm_and_trigger_clobber_bots.
    self.assertEqual("", mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_flow_with_bot_id(self, mock_confirm_and_trigger_clobber_bots):
    """Tests the main flow when a bot ID is specified."""
    mock_confirm_and_trigger_clobber_bots.return_value = [{
        'bot_id': 'test-bot-id',
        'dimensions': []
    }]
    args = [
        '--swarming-server', 'test-server', '--builder', 'test-builder',
        '--bucket', 'test-bucket', '--bot-id', 'test-bot-id'
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 0)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, False,
        'test-bot-id')
    self.assertEqual("", mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.trigger_clobber_cache')
  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_flow_xcode_clobber(self, mock_confirm_and_trigger_clobber_bots,
                                   mock_trigger):
    """Tests main flow with Xcode caches present and --xcode-action clobber."""
    mock_confirm_and_trigger_clobber_bots.return_value = [
        {
            'bot_id':
            'bot1',
            'dimensions': [
                {
                    'key': 'caches',
                    'value': ['xcode_123']  # Include an Xcode cache
                },
                {
                    'key': 'os',
                    'value': ['mac']
                },
            ]
        },
    ]
    args = [
        '--swarming-server',
        'test-server',
        '--builder',
        'test-builder',
        '--bucket',
        'test-bucket',
        '--xcode-action',
        'clobber',
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 0)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, False, None)

    # Because xcode-action is 'clobber', trigger_clobber_cache *should* be
    # called for the Xcode cache.  Verify this.
    mock_trigger.assert_called_once_with('test-server', pool, realm,
                                         'xcode_123', 'bot1', 'cache/xcode_123',
                                         False)

    # Check that the output includes the message about Xcode caches
    # and that the specific bot and cache are listed:
    self.assertIn("Some bots have Xcode caches", mock_stdout.getvalue())
    self.assertIn('"bot1": [\n    "xcode_123"\n  ]', mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.trigger_clobber_cache')
  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_flow_xcode_warn(self, mock_confirm_and_trigger_clobber_bots,
                                mock_trigger):
    """Tests main flow with Xcode caches present and --xcode-action warn."""
    mock_confirm_and_trigger_clobber_bots.return_value = [
        {
            'bot_id':
            'bot1',
            'dimensions': [
                {
                    'key': 'caches',
                    'value': ['xcode_123']  # Include an Xcode cache
                },
                {
                    'key': 'os',
                    'value': ['mac']
                },
            ]
        },
    ]
    args = [
        '--swarming-server', 'test-server', '--builder', 'test-builder',
        '--bucket', 'test-bucket', '--xcode-action', 'warn'
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 0)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, False, None)
    # When xcode-action is 'warn', trigger_clobber_cache *should* be
    # called for the Xcode cache with a warning message.  Verify this.
    mock_trigger.assert_called_once_with('test-server', pool, realm,
                                         'xcode_123', 'bot1', 'cache/xcode_123',
                                         False)

    # Check that the output includes the WARNING message
    # and that the specific bot and cache are listed:
    self.assertIn("WARNING: Some bots have Xcode caches",
                  mock_stdout.getvalue())
    self.assertIn('"bot1": [\n    "xcode_123"\n  ]', mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.trigger_clobber_cache')
  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_flow_xcode_error(self, mock_confirm_and_trigger_clobber_bots,
                                 mock_trigger):
    """Tests main flow with Xcode caches present and --xcode-action error."""
    mock_confirm_and_trigger_clobber_bots.return_value = [
        {
            'bot_id':
            'bot1',
            'dimensions': [
                {
                    'key': 'caches',
                    'value': ['xcode_123']  # Include an Xcode cache
                },
                {
                    'key': 'os',
                    'value': ['mac']
                },
            ]
        },
    ]
    args = [
        '--swarming-server', 'test-server', '--builder', 'test-builder',
        '--bucket', 'test-bucket', '--xcode-action', 'error'
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 1)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, False, None)
    #  trigger_clobber_cache should *NOT* be called.
    mock_trigger.assert_not_called()

    # Check that the output includes the ERROR message
    # and that the specific bot and cache are listed:
    self.assertIn("ERROR: Some bots have Xcode caches", mock_stdout.getvalue())
    self.assertIn('"bot1": [\n    "xcode_123"\n  ]', mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_flow_cancel(self, mock_confirm_and_trigger_clobber_bots):
    """Tests the main flow when the user cancels the operation."""
    mock_confirm_and_trigger_clobber_bots.return_value = []
    args = [
        '--swarming-server', 'test-server', '--builder', 'test-builder',
        '--bucket', 'test-bucket'
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 1)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'
    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, False, None)
    # Since confirm_and_trigger_clobber_bots handles all the user interaction and output
    # related to the cancellation, the main function shouldn't print anything.
    self.assertEqual("", mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_no_bots(self, mock_confirm_and_trigger_clobber_bots):
    """Tests the case where no bots are found."""
    mock_confirm_and_trigger_clobber_bots.return_value = []
    args = [
        '--swarming-server', 'test-server', '--builder', 'test-builder',
        '--bucket', 'test-bucket'
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 1)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, False, None)
    # We expect NO output from main itself. The main function should not print
    # anything.
    self.assertEqual("", mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.trigger_clobber_cache')
  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_dry_run_with_xcode(self, mock_confirm_and_trigger_clobber_bots,
                                   mock_trigger):
    """Tests dry run mode when Xcode caches are present."""
    mock_confirm_and_trigger_clobber_bots.return_value = [
        {
            'bot_id':
            'bot1',
            'dimensions': [{
                'key': 'caches',
                'value': ['xcode_123']  # Include an Xcode cache
            }]
        },
    ]
    args = [
        '--swarming-server',
        'test-server',
        '--builder',
        'test-builder',
        '--bucket',
        'test-bucket',
        '--xcode-action',
        'warn',
        '--dry-run',
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)
    self.assertEqual(ret, 0)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, True, None)
    mock_trigger.assert_called_once_with('test-server', pool, realm,
                                         'xcode_123', 'bot1', 'cache/xcode_123',
                                         True)

    self.assertIn("WARNING: Some bots have Xcode caches",
                  mock_stdout.getvalue())
    self.assertIn('"bot1": [\n    "xcode_123"\n  ]', mock_stdout.getvalue())

  @mock.patch('clobber_cache_utils.trigger_clobber_cache')
  @mock.patch('clobber_cache_utils.confirm_and_trigger_clobber_bots')
  def test_main_dry_run_no_xcode(self, mock_confirm_and_trigger_clobber_bots,
                                 mock_trigger):
    """Tests dry run mode when no Xcode caches are present."""
    mock_confirm_and_trigger_clobber_bots.return_value = [
        {
            'bot_id': 'bot1',
            'dimensions': [{
                'key': 'caches',
                'value': []  # No xcode cache
            }]
        },
    ]
    args = [
        '--swarming-server',
        'test-server',
        '--builder',
        'test-builder',
        '--bucket',
        'test-bucket',
        '--xcode-action',
        'warn',
        '--dry-run',
    ]
    with mock.patch('sys.stdout', new_callable=StringIO) as mock_stdout:
      ret = builder_cache_clobber.main(args)

    string_to_hash = 'chromium/test-bucket/test-builder'
    h = hashlib.sha256(string_to_hash.encode('utf-8'))
    builder_cache = 'builder_%s_v2' % (h.hexdigest())
    pool = 'luci.chromium.test-bucket'
    realm = 'chromium:test-bucket'
    mount_rel_path = 'cache/builder'

    mock_confirm_and_trigger_clobber_bots.assert_called_with(
        'test-server', pool, realm, builder_cache, mount_rel_path, True, None)
    mock_trigger.assert_not_called()
    self.assertEqual("", mock_stdout.getvalue())
    self.assertEqual(ret, 0)


if __name__ == '__main__':
  unittest.main()

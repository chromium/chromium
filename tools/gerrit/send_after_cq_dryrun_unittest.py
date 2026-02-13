#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest.mock import MagicMock, patch
import send_after_cq_dryrun


class TestReviewMonitor(unittest.TestCase):

    def setUp(self):
        # Patch find_gerrit_client to return a dummy path
        with patch('send_after_cq_dryrun.find_gerrit_client',
                   return_value='/path/to/gerrit_client.py'):
            self.monitor = send_after_cq_dryrun.ReviewMonitor(
                issue_id='1234',
                issue_url='https://crrev.com/1234',
                host='https://chromium-review.googlesource.com',
                patchset='5',
                reviewers=['test@chromium.org'])

    def test_parse_results_all_success(self):
        results = [
            {
                'builder': {
                    'builder': 'bot1'
                },
                'status': 'SUCCESS'
            },
            {
                'builder': {
                    'builder': 'bot2'
                },
                'status': 'SUCCESS'
            },
        ]
        res = self.monitor.parse_results(results)
        self.assertTrue(res.finished)
        self.assertTrue(res.success)
        self.assertEqual(len(res.failed_builders), 0)
        self.assertIn("Success: 2/2", res.stats)

    def test_parse_results_still_running(self):
        results = [
            {
                'builder': {
                    'builder': 'bot1'
                },
                'status': 'SUCCESS'
            },
            {
                'builder': {
                    'builder': 'bot2'
                },
                'status': 'STARTED'
            },
        ]
        res = self.monitor.parse_results(results)
        self.assertFalse(res.finished)
        self.assertIn("Pending: 1", res.stats)
        self.assertEqual(res.failed_builders, [])

    def test_parse_results_with_failure_and_pending(self):
        # Even if one bot failed, it's not finished if others are still running.
        results = [
            {
                'builder': {
                    'builder': 'bot1'
                },
                'status': 'FAILURE'
            },
            {
                'builder': {
                    'builder': 'bot2'
                },
                'status': 'STARTED'
            },
        ]
        res = self.monitor.parse_results(results)
        self.assertFalse(res.finished)
        self.assertIn("Failed: 1", res.stats)
        self.assertIn("Pending: 1", res.stats)
        self.assertEqual(res.failed_builders, ['bot1'])

    def test_parse_results_with_retries(self):
        # Bot1 failed at T1, then succeeded at T2.
        results = [
            {
                'builder': {
                    'builder': 'bot1'
                },
                'status': 'FAILURE',
                'createTime': '2026-02-13T10:00:00Z'
            },
            {
                'builder': {
                    'builder': 'bot1'
                },
                'status': 'SUCCESS',
                'createTime': '2026-02-13T11:00:00Z'
            },
            {
                'builder': {
                    'builder': 'bot2'
                },
                'status': 'SUCCESS',
                'createTime': '2026-02-13T10:30:00Z'
            },
        ]
        res = self.monitor.parse_results(results)
        self.assertTrue(res.finished)
        self.assertTrue(res.success)
        self.assertIn("Success: 2/2", res.stats)

    def test_parse_results_missing_builder(self):
        results = [{'status': 'SUCCESS'}]
        with self.assertRaisesRegex(ValueError, "missing builder name"):
            self.monitor.parse_results(results)

    @patch('send_after_cq_dryrun.time.sleep', return_value=None)
    @patch('send_after_cq_dryrun.time.time')
    def test_monitor_waits_for_cq_retry(self, mock_time, mock_sleep):
        # Mocking time to avoid timeout
        mock_time.side_effect = [0, 10, 20, 30, 40, 50]

        # 1st call: bot failed, but CQ label is still 1.
        # 2nd call: bot succeeded.
        self.monitor.get_try_results = MagicMock(side_effect=[
            [{
                'builder': {
                    'builder': 'bot1'
                },
                'status': 'FAILURE'
            }],
            [{
                'builder': {
                    'builder': 'bot1'
                },
                'status': 'SUCCESS'
            }],
        ])
        # Initial check, after 1st try failure, then success
        self.monitor.get_cq_label = MagicMock(side_effect=[1, 1, 0])
        self.monitor.set_wip = MagicMock()
        self.monitor.trigger_dry_run = MagicMock()
        self.monitor.add_reviewer = MagicMock()
        self.monitor.set_ready = MagicMock()

        self.monitor.monitor()

        self.assertEqual(self.monitor.get_try_results.call_count, 2)
        self.monitor.set_ready.assert_called_once()


if __name__ == '__main__':
    unittest.main()

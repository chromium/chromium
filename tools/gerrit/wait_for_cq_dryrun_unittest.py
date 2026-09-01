#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
from unittest.mock import MagicMock, patch
import wait_for_cq_dryrun


class TestCqDryRunWaiter(unittest.TestCase):
    def setUp(self):
        # Patch find_gerrit_client to return a dummy path
        with patch(
            'wait_for_cq_dryrun.find_gerrit_client',
            return_value='/path/to/gerrit_client.py',
        ):
            self.waiter = wait_for_cq_dryrun.CqDryRunWaiter(
                issue_id='1234',
                issue_url='https://crrev.com/1234',
                host='https://chromium-review.googlesource.com',
                patchset='5',
            )

    def test_parse_results_all_success(self):
        results = [
            {'builder': {'builder': 'bot1'}, 'status': 'SUCCESS'},
            {'builder': {'builder': 'bot2'}, 'status': 'SUCCESS'},
        ]
        res = self.waiter.parse_results(results)
        self.assertTrue(res.finished)
        self.assertTrue(res.success)
        self.assertEqual(len(res.failed_builders), 0)
        self.assertIn("Success: 2/2", res.stats)

    def test_parse_results_still_running(self):
        results = [
            {'builder': {'builder': 'bot1'}, 'status': 'SUCCESS'},
            {'builder': {'builder': 'bot2'}, 'status': 'STARTED'},
        ]
        res = self.waiter.parse_results(results)
        self.assertFalse(res.finished)
        self.assertIn("Pending: 1", res.stats)
        self.assertEqual(res.failed_builders, [])

    def test_parse_results_with_failure_and_pending(self):
        results = [
            {
                'builder': {'builder': 'bot1'},
                'status': 'FAILURE',
                'tags': [{'key': 'user_agent', 'value': 'cq'}],
            },
            {'builder': {'builder': 'bot2'}, 'status': 'STARTED'},
        ]
        res = self.waiter.parse_results(results)
        self.assertFalse(res.finished)
        self.assertIn("Failed: 1", res.stats)
        self.assertIn("Pending: 1", res.stats)
        self.assertEqual(res.failed_builders, ['bot1 (PS 5)'])

    def test_parse_results_with_retries(self):
        results = [
            {
                'builder': {'builder': 'bot1'},
                'status': 'FAILURE',
                'createTime': '2026-02-13T10:00:00Z',
            },
            {
                'builder': {'builder': 'bot1'},
                'status': 'SUCCESS',
                'createTime': '2026-02-13T11:00:00Z',
            },
            {
                'builder': {'builder': 'bot2'},
                'status': 'SUCCESS',
                'createTime': '2026-02-13T10:30:00Z',
            },
        ]
        res = self.waiter.parse_results(results)
        self.assertTrue(res.finished)
        self.assertTrue(res.success)
        self.assertIn("Success: 2/2", res.stats)

    def test_parse_results_missing_builder(self):
        results = [{'status': 'SUCCESS'}]
        with self.assertRaisesRegex(ValueError, "missing builder name"):
            self.waiter.parse_results(results)

    @patch('wait_for_cq_dryrun.time.sleep', return_value=None)
    @patch('wait_for_cq_dryrun.time.time')
    def test_wait_waits_for_cq_retry(self, mock_time, mock_sleep):
        mock_time.side_effect = [0, 10, 20, 30, 40, 50]

        # 1st call: bot failed, but CQ label is still 1.
        # 2nd call: bot succeeded.
        self.waiter.get_all_try_results = MagicMock(
            side_effect=[
                [
                    {
                        'builder': {'builder': 'bot1'},
                        'status': 'FAILURE',
                        'tags': [{'key': 'user_agent', 'value': 'cq'}],
                    }
                ],
                [{'builder': {'builder': 'bot1'}, 'status': 'SUCCESS'}],
            ]
        )
        self.waiter.get_cq_label = MagicMock(side_effect=[1, 1, 0])
        self.waiter.trigger_dry_run = MagicMock()

        res = self.waiter.wait()

        self.assertEqual(self.waiter.get_all_try_results.call_count, 2)
        self.assertTrue(res)

    @patch('json.load')
    @patch('wait_for_cq_dryrun.run_command')
    def test_get_all_try_results(self, mock_run_command, mock_json_load):
        mock_json_load.return_value = {
            "revisions": {"rev1": {"_number": 1}, "rev2": {"_number": 2}}
        }

        ps1_resp = json.dumps(
            [
                {
                    "builder": {"builder": "bot1"},
                    "status": "FAILURE",
                    "createTime": "T1",
                }
            ]
        )
        ps2_resp = json.dumps(
            [
                {
                    "builder": {"builder": "bot1"},
                    "status": "SUCCESS",
                    "createTime": "T2",
                }
            ]
        )

        mock_run_command.side_effect = [
            ("", 0),  # For Gerrit query
            (ps1_resp, 0),  # For PS 1 results
            (ps2_resp, 0),  # For PS 2 results
        ]

        results = self.waiter.get_all_try_results()

        self.assertEqual(mock_run_command.call_count, 3)
        self.assertEqual(len(results), 2)
        self.assertEqual(results[0]["status"], "FAILURE")
        self.assertEqual(results[1]["status"], "SUCCESS")

    def test_parse_results_newest_patchset_per_builder(self):
        results = [
            {
                'builder': {'builder': 'builder_A'},
                'status': 'FAILURE',
                'createTime': '2026-02-13T10:00:00Z',
                'tags': [
                    {
                        'key': 'buildset',
                        'value': (
                            'patch/gerrit/chromium-review.googlesource.com/'
                            '7793289/1'
                        ),
                    },
                    {'key': 'user_agent', 'value': 'cq'},
                ],
            },
            {
                'builder': {'builder': 'builder_A'},
                'status': 'SUCCESS',
                'createTime': '2026-02-13T11:00:00Z',
                'tags': [
                    {
                        'key': 'buildset',
                        'value': (
                            'patch/gerrit/chromium-review.googlesource.com/'
                            '7793289/2'
                        ),
                    },
                    {'key': 'user_agent', 'value': 'cq'},
                ],
            },
            {
                'builder': {'builder': 'builder_B'},
                'status': 'FAILURE',
                'createTime': '2026-02-13T10:30:00Z',
                'tags': [
                    {
                        'key': 'buildset',
                        'value': (
                            'patch/gerrit/chromium-review.googlesource.com/'
                            '7793289/1'
                        ),
                    },
                    {'key': 'user_agent', 'value': 'cq'},
                ],
            },
        ]

        self.waiter.patchset = '2'

        res = self.waiter.parse_results(results)

        self.assertTrue(res.finished)
        self.assertFalse(res.success)
        self.assertEqual(len(res.failed_builders), 1)
        self.assertIn('builder_B (PS 1)', res.failed_builders)
        self.assertIn("Success: 1/2", res.stats)
        self.assertIn("Failed: 1", res.stats)

    def test_parse_results_ignore_optional_failures(self):
        results = [
            {
                'builder': {'builder': 'builder_A'},
                'status': 'FAILURE',
                'createTime': '2026-02-13T10:00:00Z',
                'tags': [
                    {
                        'key': 'buildset',
                        'value': (
                            'patch/gerrit/chromium-review.googlesource.com/'
                            '7793289/1'
                        ),
                    },
                    {'key': 'user_agent', 'value': 'cq'},
                ],
            },
            {
                'builder': {'builder': 'builder_A'},
                'status': 'SUCCESS',
                'createTime': '2026-02-13T11:00:00Z',
                'tags': [
                    {
                        'key': 'buildset',
                        'value': (
                            'patch/gerrit/chromium-review.googlesource.com/'
                            '7793289/2'
                        ),
                    },
                    {'key': 'user_agent', 'value': 'cq'},
                ],
            },
            {
                'builder': {'builder': 'builder_B'},
                'status': 'FAILURE',
                'createTime': '2026-02-13T10:30:00Z',
                'tags': [
                    {
                        'key': 'buildset',
                        'value': (
                            'patch/gerrit/chromium-review.googlesource.com/'
                            '7793289/1'
                        ),
                    },
                    {'key': 'user_agent', 'value': 'gerrit'},
                ],
            },
        ]

        self.waiter.patchset = '2'

        res = self.waiter.parse_results(results)

        self.assertTrue(res.finished)
        self.assertTrue(res.success)
        self.assertEqual(len(res.failed_builders), 0)
        self.assertIn("Success: 1/2", res.stats)
        self.assertIn("Failed: 0", res.stats)
        self.assertIn("Ignored: 1", res.stats)


if __name__ == '__main__':
    unittest.main()

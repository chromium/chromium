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
        with patch(
            'wait_for_cq_dryrun.find_gerrit_client',
            return_value='/path/to/gerrit_client.py',
        ):
            self.monitor = send_after_cq_dryrun.ReviewMonitor(
                issue_id='1234',
                issue_url='https://crrev.com/1234',
                host='https://chromium-review.googlesource.com',
                patchset='5',
                reviewers=['test@chromium.org'],
            )

    @patch('send_after_cq_dryrun.ReviewMonitor._run_gerrit_command')
    def test_add_reviewer(self, mock_run_gerrit):
        self.monitor.add_reviewer('reviewer@chromium.org')
        mock_run_gerrit.assert_called_once()
        cmd = mock_run_gerrit.call_args[0][0]
        self.assertIn('/changes/1234/reviewers', cmd)

    @patch('send_after_cq_dryrun.ReviewMonitor._run_gerrit_command')
    def test_set_wip(self, mock_run_gerrit):
        self.monitor.set_wip(message='Setting WIP')
        mock_run_gerrit.assert_called_once()
        cmd = mock_run_gerrit.call_args[0][0]
        self.assertIn('/changes/1234/wip', cmd)

    @patch('send_after_cq_dryrun.ReviewMonitor._run_gerrit_command')
    def test_set_ready(self, mock_run_gerrit):
        self.monitor.set_ready(message='Ready for review')
        mock_run_gerrit.assert_called_once()
        cmd = mock_run_gerrit.call_args[0][0]
        self.assertIn('/changes/1234/ready', cmd)

    def test_monitor_success_flow(self):
        self.monitor.set_wip = MagicMock()
        self.monitor.wait = MagicMock(return_value=True)
        self.monitor.add_reviewer = MagicMock()
        self.monitor.set_ready = MagicMock()

        self.monitor.monitor()

        self.monitor.set_wip.assert_called_once()
        self.monitor.wait.assert_called_once()
        self.monitor.add_reviewer.assert_called_once_with('test@chromium.org')
        self.monitor.set_ready.assert_called_once()

    def test_monitor_failure_flow(self):
        self.monitor.set_wip = MagicMock()
        self.monitor.wait = MagicMock(return_value=False)
        self.monitor.add_reviewer = MagicMock()
        self.monitor.set_ready = MagicMock()

        with self.assertRaises(SystemExit):
            self.monitor.monitor()

        self.monitor.set_wip.assert_called_once()
        self.monitor.wait.assert_called_once()
        self.monitor.add_reviewer.assert_not_called()
        self.monitor.set_ready.assert_not_called()


if __name__ == '__main__':
    unittest.main()

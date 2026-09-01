#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import re
import sys

import wait_for_cq_dryrun


class ReviewMonitor(wait_for_cq_dryrun.CqDryRunWaiter):
    def __init__(
        self,
        issue_id,
        issue_url,
        host,
        patchset,
        reviewers,
        dry_run=False,
        verbose=False,
        is_bg=False,
    ):
        super().__init__(
            issue_id=issue_id,
            issue_url=issue_url,
            host=host,
            patchset=patchset,
            dry_run=dry_run,
            verbose=verbose,
            is_bg=is_bg,
        )
        self.reviewers = reviewers

    def add_reviewer(self, reviewer):
        print(f"   Adding reviewer: {reviewer}")
        body = json.dumps({"reviewer": reviewer})
        cmd = [
            'vpython3',
            self.gerrit_client,
            'rawapi',
            f'--host={self.host}',
            '--method',
            'POST',
            '--path',
            f'/changes/{self.issue_id}/reviewers',
            '--body',
            body,
            '--accept_status',
            '200,204,409',
        ]
        self._run_gerrit_command(cmd)

    def set_wip(self, message=None):
        print(f"   Setting CL {self.issue_id} to WIP...")
        cmd = [
            'vpython3',
            self.gerrit_client,
            'rawapi',
            f'--host={self.host}',
            '--method',
            'POST',
            '--path',
            f'/changes/{self.issue_id}/wip',
            '--accept_status',
            '200,204,409',
        ]
        if message:
            cmd.extend(['--body', json.dumps({"message": message})])

        self._run_gerrit_command(
            cmd, ignorable_msgs=["already work in progress"]
        )

    def set_ready(self, message=None):
        print(f"   Setting CL {self.issue_id} to Ready for Review...")
        cmd = [
            'vpython3',
            self.gerrit_client,
            'rawapi',
            f'--host={self.host}',
            '--method',
            'POST',
            '--path',
            f'/changes/{self.issue_id}/ready',
            '--accept_status',
            '200,204,409',
        ]
        if message:
            cmd.extend(['--body', json.dumps({"message": message})])

        self._run_gerrit_command(
            cmd, ignorable_msgs=["already ready for review"]
        )

    def monitor(self):
        print(f"📧 Target Reviewers: {', '.join(self.reviewers)}")

        self.set_wip(
            message="Triggering and monitoring CQ dry run; will mark Ready "
            "for Review upon success (automated via send_after_cq_dryrun.py)."
        )

        if not self.wait():
            sys.exit(1)

        msg = (
            f"CQ passed! Transitioning CL {self.issue_id} "
            "to Ready for Review..."
        )
        print(f"\n\n✅ {msg}")

        for r in self.reviewers:
            self.add_reviewer(r)

        self.set_ready(
            message="CQ dry run passed! Sending for review "
            "(automated via send_after_cq_dryrun.py)."
        )


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('reviewers', nargs='*', help='Reviewer emails or LDAPs')
    parser.add_argument(
        '--patchset', help='Manually specify patchset to monitor'
    )
    parser.add_argument(
        '--dry-run', action='store_true', help='Test monitoring logic'
    )
    parser.add_argument(
        '--verbose', action='store_true', help='Print internal commands'
    )
    parser.add_argument(
        '--bg',
        action='store_true',
        help='Run monitoring in background and detach',
    )
    args = parser.parse_args()

    if not args.dry_run and not args.reviewers:
        parser.error(
            "the following arguments are required: reviewers "
            "(unless --dry-run is used)"
        )

    issue_id, issue_url, patchset, host = wait_for_cq_dryrun.get_issue_info()
    target_patchset = args.patchset or patchset
    final_reviewers = [
        r.strip()
        for r in re.split(r'[,\s]+', ' '.join(args.reviewers))
        if r.strip()
    ]

    if args.bg:
        wait_for_cq_dryrun.detach_process(issue_id, target_patchset)

    monitor = ReviewMonitor(
        issue_id=issue_id,
        issue_url=issue_url,
        host=host,
        patchset=target_patchset,
        reviewers=final_reviewers,
        dry_run=args.dry_run,
        verbose=args.verbose,
        is_bg=args.bg,
    )
    monitor.monitor()


if __name__ == "__main__":
    main()

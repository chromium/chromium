#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import subprocess
import time
import sys
import os
import re
from datetime import timedelta

# Configuration
POLL_INTERVAL_SECONDS = 60
TIMEOUT_HOURS = 4
TIMEOUT_SECONDS = TIMEOUT_HOURS * 3600


def run_command(args):
    """Runs a command and returns (stdout+stderr, returncode)."""
    result = subprocess.run(args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True)
    return result.stdout.strip(), result.returncode


def find_gerrit_client():
    for path in os.environ["PATH"].split(os.pathsep):
        candidate = os.path.join(path, "gerrit_client.py")
        if os.path.isfile(candidate):
            return candidate
    return None


def get_issue_info():
    # Use issue command as it's more direct for the ID
    issue_output, _ = run_command(['git', 'cl', 'issue'])
    if "Issue number: None" in issue_output:
        print("❌ No active CL found on this branch.")
        sys.exit(1)

    url_match = re.search(r'\((https?://[^\s)]+)\)', issue_output)
    if not url_match:
        print(f"❌ Could not find issue URL in: {issue_output}")
        sys.exit(1)

    issue_url = url_match.group(1).strip()

    try:
        # Extract host: https://chromium-review.googlesource.com
        host = 'https://' + issue_url.split('//')[1].split('/')[0].strip()
        # Extract ID: 7514529
        issue_id = issue_url.rstrip('/').split('/')[-1].strip()
    except:
        print(f"❌ Failed to parse host/ID from URL: {issue_url}")
        sys.exit(1)

    # Get patchset number - handle multiline output from depot_tools noise
    ps_output, _ = run_command(['git', 'cl', 'status', '--field=patch'])
    patchset = ""
    for line in ps_output.splitlines():
        if line.strip().isdigit():
            patchset = line.strip()
            break

    return issue_id, issue_url, patchset, host


class ReviewMonitor:

    def __init__(self,
                 issue_id,
                 host,
                 patchset,
                 reviewers,
                 dry_run=False,
                 verbose=False):
        self.issue_id = issue_id
        self.host = host
        self.patchset = patchset
        self.reviewers = reviewers
        self.dry_run = dry_run
        self.verbose = verbose
        self.gerrit_client = find_gerrit_client()
        if not self.gerrit_client:
            print("❌ Could not find gerrit_client.py in your PATH.")
            sys.exit(1)

    def get_try_results(self):
        cmd = [
            'git', 'cl', 'try-results', '--issue', self.issue_id, '--json', '-'
        ]
        if self.patchset:
            cmd.extend(['--patchset', self.patchset])

        stdout, code = run_command(cmd)
        if code != 0: return None
        try:
            return json.loads(stdout)
        except:
            return None

    def parse_results(self, results):
        if not results:
            return False, "Waiting for builds...", False, []

        total = len(results)
        success = []
        running = []
        failed = []

        for job in results:
            name = job.get('builder', {}).get('builder', 'unknown')
            status = job.get('status')
            if status == 'SUCCESS':
                success.append(name)
            elif status in ['STARTED', 'SCHEDULED', 'PENDING']:
                running.append(name)
            else:
                failed.append(name)

        stats = (f"Success: {len(success)}/{total} | "
                 f"Pending: {len(running)} | Failed: {len(failed)}")

        if len(failed) > 0:
            return True, stats, False, failed
        if len(running) == 0 and len(success) == total and total > 0:
            return True, stats, True, []

        return False, stats, False, running

    def _run_gerrit_command(self, cmd, ignorable_msgs=None):
        """Runs a gerrit_client command and handles success/failure."""
        ignorable_msgs = ignorable_msgs or []
        # 409 is the standard Gerrit conflict code, but gerrit_util often
        # wraps others into a (200) error if the response isn't JSON.
        ignorable_msgs.append("409")

        if self.verbose:
            print(f"      {' '.join(cmd)}")

        if self.dry_run:
            return

        out, code = run_command(cmd)
        if code != 0 and not any(msg in out for msg in ignorable_msgs):
            print(f"      ❌ Failed: {out}")
        else:
            print(f"      ✅ Success")

    def add_reviewer(self, reviewer):
        print(f"   Adding reviewer: {reviewer}")
        body = json.dumps({"reviewer": reviewer})
        cmd = [
            'vpython3', self.gerrit_client, 'rawapi', f'--host={self.host}',
            '--method', 'POST', '--path',
            f'/changes/{self.issue_id}/reviewers', '--body', body,
            '--accept_status', '200,204,409'
        ]
        self._run_gerrit_command(cmd)

    def set_wip(self, message=None):
        print(f"   Setting CL {self.issue_id} to WIP...")
        cmd = [
            'vpython3', self.gerrit_client, 'rawapi', f'--host={self.host}',
            '--method', 'POST', '--path', f'/changes/{self.issue_id}/wip',
            '--accept_status', '200,204,409'
        ]
        if message:
            cmd.extend(['--body', json.dumps({"message": message})])

        self._run_gerrit_command(cmd,
                                 ignorable_msgs=["already work in progress"])

    def set_ready(self, message=None):
        print(f"   Setting CL {self.issue_id} to Ready for Review...")
        cmd = [
            'vpython3', self.gerrit_client, 'rawapi', f'--host={self.host}',
            '--method', 'POST', '--path', f'/changes/{self.issue_id}/ready',
            '--accept_status', '200,204,409'
        ]
        if message:
            cmd.extend(['--body', json.dumps({"message": message})])

        self._run_gerrit_command(cmd,
                                 ignorable_msgs=["already ready for review"])

    def get_cq_label(self):
        import tempfile
        with tempfile.NamedTemporaryFile(delete=False) as f:
            temp_path = f.name

        try:
            cmd = [
                'vpython3', self.gerrit_client, 'rawapi',
                f'--host={self.host}', '--method', 'GET', '--path',
                f'/changes/{self.issue_id}/?o=LABELS', '--json_file',
                temp_path, '--accept_status', '200'
            ]
            out, code = run_command(cmd)
            if code != 0:
                return 0

            with open(temp_path, 'r') as f:
                data = json.load(f)

            cq = data.get('labels', {}).get('Commit-Queue', {})
            if 'approved' in cq: return 2
            if 'recommended' in cq: return 1

            max_v = 0
            for approval in cq.get('all', []):
                max_v = max(max_v, approval.get('value', 0))
            return max_v
        except:
            return 0
        finally:
            if os.path.exists(temp_path):
                os.remove(temp_path)

    def trigger_dry_run(self, message=None):
        print(f"   Triggering CQ Dry Run for CL {self.issue_id}...")
        review_input = {"labels": {"Commit-Queue": 1}}
        if message:
            review_input["message"] = message
        body = json.dumps(review_input)
        cmd = [
            'vpython3', self.gerrit_client, 'rawapi', f'--host={self.host}',
            '--method', 'POST', '--path',
            f'/changes/{self.issue_id}/revisions/{self.patchset}/review',
            '--body', body, '--accept_status', '200,204,409'
        ]
        self._run_gerrit_command(cmd)

    def monitor(self):
        print(f"🚀 Monitoring CQ for CL {self.issue_id} "
              f"(Patchset: {self.patchset})")
        print(f"🌐 Host: {self.host}")
        print(f"📧 Target Reviewers: {', '.join(self.reviewers)}")
        if self.dry_run:
            print("🧪 Dry run mode: Commands will be printed but not executed.")
        print(f"⏱️  Timeout: {TIMEOUT_HOURS} hours\n")

        self.set_wip(
            message=
            "[automated] Triggering and monitoring CQ dry run; will mark "
            "Ready for Review upon success (via send_after_cq_dryrun.py).")

        if self.get_cq_label() < 1:
            self.trigger_dry_run(
                message=
                "[automated] Triggering CQ dry run (via send_after_cq_dryrun.py)."
            )

        start_time = time.time()
        try:
            while True:
                elapsed_total_seconds = int(time.time() - start_time)
                if elapsed_total_seconds > TIMEOUT_SECONDS:
                    print(f"\n\n⏰ Timeout reached after {TIMEOUT_HOURS} "
                          "hours. Stopping monitoring.")
                    sys.exit(1)

                results = self.get_try_results()
                finished, stats, success, names = self.parse_results(results)

                elapsed = str(timedelta(seconds=elapsed_total_seconds))

                # Update the same line in terminal
                sys.stdout.write(
                    f"\r[{elapsed}] {stats} | "
                    f"{len(results) if results else 0} bots found...   ")
                sys.stdout.flush()

                if finished:
                    if success:
                        msg = (f"CQ passed! Transitioning CL {self.issue_id} "
                               "to Ready for Review...")
                        print(f"\n\n✅ {msg}")

                        for r in self.reviewers:
                            self.add_reviewer(r)

                        self.set_ready(
                            message="[automated] CQ dry run passed! "
                            "Sending for review (via send_after_cq_dryrun.py)."
                        )
                    else:
                        msg = f"CQ failed on: {', '.join(names)}"
                        print(f"\n\n🛑 {msg}")
                    break

                time.sleep(POLL_INTERVAL_SECONDS)
        except KeyboardInterrupt:
            print("\n\n👋 Monitoring cancelled.")


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('reviewers',
                        nargs='*',
                        help='Reviewer emails or LDAPs')
    parser.add_argument('--patchset',
                        help='Manually specify patchset to monitor')
    parser.add_argument('--dry-run',
                        action='store_true',
                        help='Test monitoring logic')
    parser.add_argument('--verbose',
                        action='store_true',
                        help='Print internal commands')
    args = parser.parse_args()

    if not args.dry_run and not args.reviewers:
        parser.error("the following arguments are required: reviewers "
                     "(unless --dry-run is used)")

    issue_id, issue_url, patchset, host = get_issue_info()
    target_patchset = args.patchset or patchset
    final_reviewers = [
        r.strip() for r in re.split(r'[,\s]+', ' '.join(args.reviewers))
        if r.strip()
    ]

    monitor = ReviewMonitor(issue_id=issue_id,
                            host=host,
                            patchset=target_patchset,
                            reviewers=final_reviewers,
                            dry_run=args.dry_run,
                            verbose=args.verbose)
    monitor.monitor()


if __name__ == "__main__":
    main()

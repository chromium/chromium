#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pulls the latest revisions of the wpt tooling."""

import os
import shutil
import subprocess
import sys
import time

BUG_QUERY_URLS = ["https://issues.chromium.org/issues?"
                  "q=status:open%20componentid:1456176%20customfield1223031:%20WPT-Tooling-Roll",
                  "https://issues.chromium.org/issues?"
                  "q=status:open%20componentid:1456176%20customfield1223031:%20WPT-JS-Roll"]


def main():
    current_branch = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'])
    current_branch = current_branch.rstrip().decode('utf-8')
    print("Roll wpt on branch: %s" % current_branch)
    print("Are there outstanding bugs at %s (Y/n)?" % BUG_QUERY_URLS[0],
          end='', flush=True)
    yesno = sys.stdin.readline().strip()
    if yesno not in ['N', 'n']:
        return 1

    remote_head = subprocess.check_output(['git',
                                           'ls-remote',
                                           'https://github.com/web-platform-tests/wpt',
                                           'refs/heads/master'])
    remote_head = remote_head.rstrip().decode('utf-8').split()
    remote_head = remote_head[0]
    print("Roll to remote head: %s" % remote_head)

    pattern = "s/^Version: .*$/Version: %s/g" % remote_head
    path_to_wpt_tools_dir = os.path.abspath(os.path.dirname(__file__))
    path_to_readme = os.path.join(path_to_wpt_tools_dir, "README.chromium")

    # Update Version in //third_party/wpt_tools/README.chromium
    # This program only works on linux for now, as sed has slightly
    # different format on mac
    print("Update commit hash code for %s" % path_to_readme)
    subprocess.check_call(["sed", "-i", pattern, path_to_readme])

    path_to_checkout = os.path.join(path_to_wpt_tools_dir, "checkout.sh")
    print("Call %s\n" % path_to_checkout)
    subprocess.check_output([path_to_checkout, remote_head])

    change_files = subprocess.check_output(['git',
                                            'diff',
                                            'HEAD',
                                            '--no-renames',
                                            '--name-only'])
    change_files = change_files.decode('utf-8').strip()
    if change_files == '':
        print("No tooling changes to roll!")
        return 0

    subprocess.check_call(['git', 'add', path_to_wpt_tools_dir])
    # Trigger linux-blink-rel should be good enough because webdriver tests are also
    # using Wptrunner now
    wpt_try_bots = ["linux-blink-rel"]
    upstream_url = "https://github.com/web-platform-tests/wpt"
    message = "Roll wpt tooling\n\nThis rolls wpt to latest commit at\n%s.\n" % upstream_url
    message += "REMOTE-WPT-HEAD: %s\n\n" % remote_head
    message += "Cq-Include-Trybots: luci.chromium.try:%s\n" % ','.join(wpt_try_bots)
    subprocess.check_call(['git', 'commit', '-m', message])
    subprocess.check_call(['git',
                           'cl',
                           'upload',
                           '--enable-auto-submit',
                           '--cq-dry-run',
                           '--bypass-hooks',
                           '-f'])

    output = subprocess.check_output(['git', 'cl', 'issue']).decode('utf-8')
    issue_number = output.strip().split()[2]
    print("\nCL uploaded to https://chromium-review.googlesource.com/%s" % issue_number)
    print("Please monitor the results on WPT try bots.")
    print("One common failure is that some dependency is not satisfied.")
    print("Please consider update WPTIncludeList in such case.")

    print("\n\nNow roll wpt javascript")
    print("Are there outstanding bugs at %s (Y/n)?" % BUG_QUERY_URLS[1],
          end='', flush=True)
    yesno = sys.stdin.readline().strip()
    if yesno not in ['N', 'n']:
        return 1
    javascript_branch = "%s-%d" % (current_branch, int(time.time()))
    print("Roll wpt javascript on branch: %s" % javascript_branch)
    subprocess.check_call(['git', 'new-branch', javascript_branch])
    files_to_roll = ["testharness.js", "testdriver.js", "testdriver-actions.js", "check-layout-th.js"]
    chromium_src_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
    source_dir = os.path.join(chromium_src_dir,
                              "third_party",
                              "blink",
                              "web_tests",
                              "external",
                              "wpt",
                              "resources")
    dst_dir = os.path.join(chromium_src_dir,
                           "third_party",
                           "blink",
                           "web_tests",
                           "resources")
    for f in files_to_roll:
        shutil.copy(os.path.join(source_dir, f),
                    os.path.join(dst_dir, f))

    change_files = subprocess.check_output(['git',
                                            'diff',
                                            'HEAD',
                                            '--no-renames',
                                            '--name-only'])
    change_files = change_files.decode('utf-8').strip()
    if change_files == '':
        print("No javascript changes to roll!")
    else:
        for f in files_to_roll:
            subprocess.check_call(['git', 'add', os.path.join(dst_dir, f)])
        message = ("Roll wpt javascript\n\nThis rolls wpt javascript to latest commit at\n"
                   "%s.\n" % upstream_url)
        subprocess.check_call(['git', 'commit', '-m', message])
        subprocess.check_call(['git',
                               'cl',
                               'upload',
                               '--enable-auto-submit',
                               '--cq-dry-run',
                               '--bypass-hooks',
                               '-f'])
        output = subprocess.check_output(['git', 'cl', 'issue']).decode('utf-8')
        issue_number = output.strip().split()[2]
        print("\nCL uploaded to https://chromium-review.googlesource.com/%s" % issue_number)


    print("Deleting branch %s.\nCurrent branch is %s." % (javascript_branch, current_branch))
    subprocess.check_call(['git', 'checkout', current_branch])
    subprocess.check_call(['git', 'branch', '-D', javascript_branch])
    return 0

if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script can help with creating draft comments in a Gerrit code review.

The script needs to be invoked from a `git` branch that 1) is associated
with a Gerrit review, and 2) has an explicitly defined upstream branch.
The script is typically invoked either on 1) an update branch created by
`//tools/crates/create_update_cl.py`, or 2) a manually crated branch that adds a
new crate to Chromium.
"""

import argparse
import json
import os
import re
import shutil
import sys
from create_update_cl import Git

DEPOT_TOOLS_PATH = os.path.dirname(shutil.which('gclient'))
sys.path.insert(0, DEPOT_TOOLS_PATH)
from gerrit_util import CreateHttpConn, ReadHttpJsonResponse


class DraftCreator:

    def __init__(self):
        gerrit_json = json.loads(Git('cl', 'issue', '--json=-').splitlines()[1])
        self.gerrit_host = gerrit_json['gerrit_host']
        self.gerrit_issue = str(gerrit_json['issue'])

    def CreateDraft(self, path, line, message):
        """ Calls the following REST API:
        https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#create-draft
        """

        url_path = f'changes/{self.gerrit_issue}/revisions/current/drafts'
        body: Dict[str, Any] = {
            'path': str(path),
            'line': int(line),
            'message': message,
            'unresolved': True,
        }
        print(f'Creating a comment for {path}:{line}')
        conn = CreateHttpConn(self.gerrit_host,
                              url_path,
                              reqtype='PUT',
                              body=body)
        response = ReadHttpJsonResponse(conn, accept_statuses=[200, 201])


def main():
    parser = argparse.ArgumentParser(
        description='Create draft comments in a Gerrit code review')
    parser.add_argument('--comment',
                        default='TODO: `unsafe` review',
                        help='Text of the comment')
    parser.add_argument(
        '--trigger-regex',
        default='\\bunsafe\\b',
        help='Text/substring of lines that need new draft comments')
    parser.add_argument(
        '--path-regex',
        default='third_party/rust/chromium_crates_io/vendor/.*\\.rs$',
        help='Text/substring of lines that need new draft comments')
    args = parser.parse_args()

    trigger_regex = re.compile(args.trigger_regex)
    path_regex = re.compile(args.path_regex)

    curr_branch = Git('rev-parse', '--abbrev-ref', 'HEAD').strip()
    upstream_branch = Git('rev-parse', '--abbrev-ref', '--symbolic-full-name',
                          '@{u}').strip()
    file_list = Git('diff', '--name-only', '--diff-filter=ACMR',
                    f'{upstream_branch}..{curr_branch}')

    draft_creator = DraftCreator()
    for file_path in file_list.splitlines():
        file_path = file_path.strip()
        if not path_regex.search(file_path): continue
        with open(file_path) as file:
            lines = [line.rstrip() for line in file]
        for line_number in range(len(lines)):
            line = lines[line_number]
            line_number = line_number + 1  # Line numbers in Gerrit start at 1
            if not trigger_regex.search(line): continue
            draft_creator.CreateDraft(file_path, line_number, args.comment)

    return 0


if __name__ == '__main__':
    sys.exit(main())

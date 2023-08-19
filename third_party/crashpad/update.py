#!/usr/bin/env python3
# coding: utf-8

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import os
import re
import shlex
import subprocess
import sys
import tempfile
import textwrap

if sys.version_info[0] < 3:
  input = raw_input


IS_WINDOWS = sys.platform.startswith('win')


def SubprocessCheckCall0Or1(args):
    """Like subprocss.check_call(), but allows a return code of 1.

    Returns True if the subprocess exits with code 0, False if it exits with
    code 1, and re-raises the subprocess.check_call() exception otherwise.
    """
    try:
        subprocess.check_call(args, shell=IS_WINDOWS)
    except subprocess.CalledProcessError as e:
        if e.returncode != 1:
            raise
        return False

    return True


def GitMergeBaseIsAncestor(ancestor, descendant):
    """Determines whether |ancestor| is an ancestor of |descendant|.
    """
    return SubprocessCheckCall0Or1(
        ['git', 'merge-base', '--is-ancestor', ancestor, descendant])


def main(args):
    parser = argparse.ArgumentParser(
        description='Update the in-tree copy of an imported project')
    parser.add_argument(
        '--repository',
        default='https://chromium.googlesource.com/crashpad/crashpad',
        help='The imported project\'s remote fetch URL',
        metavar='URL')
    parser.add_argument(
        '--subtree',
        default='third_party/crashpad/crashpad',
        help='The imported project\'s location in this project\'s tree',
        metavar='PATH')
    parser.add_argument(
        '--update-to',
        default='FETCH_HEAD',
        help='What to update the imported project to',
        metavar='COMMITISH')
    parser.add_argument(
        '--fetch-ref',
        default='HEAD',
        help='The remote ref to fetch',
        metavar='REF')
    parser.add_argument(
        '--readme',
        help='The README.chromium file describing the imported project',
        metavar='FILE',
        dest='readme_path')
    parser.add_argument(
        '--exclude',
        default=['codereview.settings', 'infra'],
        action='append',
        help='Files to exclude from the imported copy',
        metavar='PATH')
    parsed = parser.parse_args(args)

    original_head = (
        subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                shell=IS_WINDOWS).rstrip())
    original_head = original_head.decode('utf-8')

    # Read the README, because that’s what it’s for. Extract some things from
    # it, and save it to be able to update it later.
    readme_path = (parsed.readme_path or
                   os.path.join(os.path.dirname(__file__ or '.'),
                                'README.chromium'))
    readme_content_old = open(readme_path, 'rb').read().decode('utf-8')

    project_name_match = re.search(
        r'^Name:\s+(.*)$', readme_content_old, re.MULTILINE)
    project_name = project_name_match.group(1)

    # Extract the original commit hash from the README.
    revision_match = re.search(r'^Revision:\s+([0-9a-fA-F]{40})($|\s)',
                               readme_content_old,
                               re.MULTILINE)
    revision_old = revision_match.group(1)

    subprocess.check_call(['git', 'fetch', parsed.repository, parsed.fetch_ref],
                          shell=IS_WINDOWS)

    # Make sure that parsed.update_to is an ancestor of FETCH_HEAD, and
    # revision_old is an ancestor of parsed.update_to. This prevents the use of
    # hashes that are known to git but that don’t make sense in the context of
    # the update operation.
    if not GitMergeBaseIsAncestor(parsed.update_to, 'FETCH_HEAD'):
        raise Exception('update_to is not an ancestor of FETCH_HEAD',
                        parsed.update_to,
                        'FETCH_HEAD')
    if not GitMergeBaseIsAncestor(revision_old, parsed.update_to):
        raise Exception('revision_old is not an ancestor of update_to',
                        revision_old,
                        parsed.update_to)

    # git-filter-branch needs a ref to update. It’s not enough to just tell it
    # to operate on a range of commits ending at parsed.update_to, because
    # parsed.update_to is a commit hash that can’t be updated to point to
    # anything else.
    subprocess.check_call(['git', 'update-ref', 'UPDATE_TO', parsed.update_to],
                          shell=IS_WINDOWS)

    # Filter the range being updated over to exclude files that ought to be
    # missing. This points UPDATE_TO to the rewritten (filtered) version.
    # git-filter-branch insists on running from the top level of the working
    # tree.
    toplevel = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'],
                                       shell=IS_WINDOWS).rstrip()
    subprocess.check_call(
        ['git',
         'filter-branch',
         '--force',
         '--index-filter',
         'git rm -r --cached --ignore-unmatch ' +
             ' '.join(shlex.quote(path) for path in parsed.exclude),
         revision_old + '..UPDATE_TO'],
        cwd=toplevel,
        shell=IS_WINDOWS)

    # git-filter-branch saved a copy of the original UPDATE_TO at
    # original/UPDATE_TO, but this isn’t useful because it refers to the same
    # thing as parsed.update_to, which is already known.
    subprocess.check_call(
        ['git', 'update-ref', '-d', 'refs/original/UPDATE_TO'],
        shell=IS_WINDOWS)

    filtered_update_range = revision_old + '..UPDATE_TO'
    unfiltered_update_range = revision_old + '..' + parsed.update_to

    # This cherry-picks each change in the window from the filtered view of the
    # upstream project into the current branch.
    assisted_cherry_pick = False
    try:
        if not SubprocessCheckCall0Or1(['git',
                                        'cherry-pick',
                                        '--keep-redundant-commits',
                                        '--strategy=subtree',
                                        '-Xsubtree=' + parsed.subtree,
                                        '-x',
                                        filtered_update_range]):
            assisted_cherry_pick = True
            print("""
Please fix the errors above and run "git cherry-pick --continue".
Press Enter when "git cherry-pick" completes.
You may use a new shell for this, or ^Z if job control is available.
Press ^C to abort.
""", file=sys.stderr)
            input()
    except:
        # ^C, signal, or something else.
        print('Aborting...', file=sys.stderr)
        subprocess.call(['git', 'cherry-pick', '--abort'], shell=IS_WINDOWS)
        raise

    # Get an abbreviated hash and subject line for each commit in the window,
    # sorted in chronological order. Use the unfiltered view so that the commit
    # hashes are recognizable.
    log_lines = subprocess.check_output(
        ['git',
         '-c',
         'core.abbrev=12',
         'log',
         '--abbrev-commit',
         '--pretty=oneline',
         '--reverse',
         unfiltered_update_range],
         shell=IS_WINDOWS).decode('utf-8').splitlines(False)

    if assisted_cherry_pick:
        # If the user had to help, count the number of cherry-picked commits,
        # expecting it to match.
        cherry_picked_commits = int(subprocess.check_output(
            ['git', 'rev-list', '--count', original_head + '..HEAD'],
            shell=IS_WINDOWS))
        if cherry_picked_commits != len(log_lines):
            print('Something smells fishy, aborting anyway...', file=sys.stderr)
            subprocess.call(['git', 'cherry-pick', '--abort'], shell=IS_WINDOWS)
            raise Exception('not all commits were cherry-picked',
                            len(log_lines),
                            cherry_picked_commits)

    # Make a nice commit message. Start with the full commit hash.
    revision_new = subprocess.check_output(
        ['git', 'rev-parse', parsed.update_to],
        shell=IS_WINDOWS).decode('utf-8').rstrip()
    new_message = u'Update ' + project_name + ' to ' + revision_new + '\n\n'

    # Wrap everything to 72 characters, with a hanging indent.
    wrapper = textwrap.TextWrapper(width=72, subsequent_indent = ' ' * 13)
    for line in log_lines:
        # Strip trailing periods from subjects.
        if line.endswith('.'):
            line = line[:-1]

        # If any subjects have what look like commit hashes in them, truncate
        # them to 12 characters.
        line = re.sub(r'(\s)([0-9a-fA-F]{12})([0-9a-fA-F]{28})($|\s)',
                      r'\1\2\4',
                      line)

        new_message += '\n'.join(wrapper.wrap(line)) + '\n'

    # Update the README with the new hash.
    readme_content_new = re.sub(
        r'^(Revision:\s+)([0-9a-fA-F]{40})($|\s.*?$)',
        r'\g<1>' + revision_new,
        readme_content_old,
        1,
        re.MULTILINE)

    # If the in-tree copy has no changes relative to the upstream, clear the
    # “Local Modifications” section of the README.
    has_local_modifications = True
    if SubprocessCheckCall0Or1(['git',
                                'diff-tree',
                                '--quiet',
                                'UPDATE_TO',
                                'HEAD:' + parsed.subtree]):
        has_local_modifications = False

        if not parsed.exclude:
            modifications = 'None.\n'
        elif len(parsed.exclude) == 1:
            modifications = (
                ' - %s has been excluded.\n' % parsed.exclude[0])
        else:
            modifications = (
                ' - The following files have been excluded:\n')
            for excluded in sorted(parsed.exclude):
                modifications += '    - ' + excluded + '\n'
        readme_content_new = re.sub(r'\nLocal Modifications:\n.*$',
                                    '\nLocal Modifications:\n' + modifications,
                                    readme_content_new,
                                    1,
                                    re.DOTALL)

    # The UPDATE_TO ref is no longer useful.
    subprocess.check_call(['git', 'update-ref', '-d', 'UPDATE_TO'],
                          shell=IS_WINDOWS)

    # This soft-reset causes all of the cherry-picks to show up as staged, which
    # will have the effect of squashing them along with the README update when
    # committed below.
    subprocess.check_call(['git', 'reset', '--soft', original_head],
                          shell=IS_WINDOWS)

    # Write the new README.
    open(readme_path, 'wb').write(readme_content_new.encode('utf-8'))

    # Commit everything.
    subprocess.check_call(['git', 'add', readme_path], shell=IS_WINDOWS)

    try:
        commit_message_name = None
        with tempfile.NamedTemporaryFile(mode='wb',
                                         delete=False) as commit_message_f:
            commit_message_name = commit_message_f.name
            commit_message_f.write(new_message.encode('utf-8'))
        subprocess.check_call(['git',
                               'commit', '--file=' + commit_message_name],
                              shell=IS_WINDOWS)
    finally:
        if commit_message_name:
            os.unlink(commit_message_name)

    if has_local_modifications:
        print('Remember to check the Local Modifications section in ' +
              readme_path, file=sys.stderr)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

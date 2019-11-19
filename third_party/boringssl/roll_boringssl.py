#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rolls third_party/boringssl/src in DEPS and updates generated build files."""

import os
import os.path
import shutil
import subprocess
import sys


SCRIPT_PATH = os.path.abspath(__file__)
SRC_PATH = os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_PATH)))
DEPS_PATH = os.path.join(SRC_PATH, 'DEPS')
BORINGSSL_PATH = os.path.join(SRC_PATH, 'third_party', 'boringssl')
BORINGSSL_SRC_PATH = os.path.join(BORINGSSL_PATH, 'src')

if not os.path.isfile(DEPS_PATH) or not os.path.isdir(BORINGSSL_SRC_PATH):
  raise Exception('Could not find Chromium checkout')

# Pull OS_ARCH_COMBOS out of the BoringSSL script.
sys.path.append(os.path.join(BORINGSSL_SRC_PATH, 'util'))
import generate_build_files

GENERATED_FILES = [
    'BUILD.generated.gni',
    'BUILD.generated_tests.gni',
    'err_data.c',
]


def IsPristine(repo):
  """Returns True if a git checkout is pristine."""
  cmd = ['git', 'diff', '--ignore-submodules']
  return not (subprocess.check_output(cmd, cwd=repo).strip() or
              subprocess.check_output(cmd + ['--cached'], cwd=repo).strip())


def RevParse(repo, rev):
  """Resolves a string to a git commit."""
  return subprocess.check_output(['git', 'rev-parse', rev], cwd=repo).strip()


def UpdateDEPS(deps, from_hash, to_hash):
  """Updates all references of |from_hash| to |to_hash| in |deps|."""
  with open(deps, 'rb') as f:
    contents = f.read()
    if from_hash not in contents:
      raise Exception('%s not in DEPS' % from_hash)
  contents = contents.replace(from_hash, to_hash)
  with open(deps, 'wb') as f:
    f.write(contents)


def Log(repo, revspec):
  """Returns the commits in |repo| covered by |revspec|."""
  data = subprocess.check_output(['git', 'log', '--pretty=raw', revspec],
                                 cwd=repo)
  commits = []
  chunks = data.split('\n\n')
  if len(chunks) % 2 != 0:
    raise ValueError('Invalid log format')
  for i in range(0, len(chunks), 2):
    commit = {}
    # Parse commit properties.
    for line in chunks[i].split('\n'):
      name, value = line.split(' ', 1)
      commit[name] = value
    if 'commit' not in commit:
      raise ValueError('Missing commit line')
    # Parse commit message.
    message = ""
    lines = chunks[i+1].split('\n')
    # Removing the trailing empty entry.
    if lines and not lines[-1]:
      lines.pop()
    for line in lines:
      INDENT = '    '
      if not line.startswith(INDENT):
        raise ValueError('Missing indent')
      message += line[len(INDENT):] + '\n'
    commit['message'] = message
    commits.append(commit)
  return commits


def FormatCommit(commit):
  """Returns a commit formatted into a single line."""
  rev = commit['commit'][:9]
  line, _ = commit['message'].split('\n', 1)
  return '%s %s' % (rev, line)


def main():
  if len(sys.argv) > 2:
    sys.stderr.write('Usage: %s [COMMIT]' % sys.argv[0])
    return 1

  if not IsPristine(SRC_PATH):
    print >>sys.stderr, 'Chromium checkout not pristine.'
    return 0
  if not IsPristine(BORINGSSL_SRC_PATH):
    print >>sys.stderr, 'BoringSSL checkout not pristine.'
    return 0

  if len(sys.argv) > 1:
    new_head = RevParse(BORINGSSL_SRC_PATH, sys.argv[1])
  else:
    subprocess.check_call(['git', 'fetch', 'origin'], cwd=BORINGSSL_SRC_PATH)
    new_head = RevParse(BORINGSSL_SRC_PATH, 'origin/master')

  old_head = RevParse(BORINGSSL_SRC_PATH, 'HEAD')
  if old_head == new_head:
    print 'BoringSSL already up to date.'
    return 0

  print 'Rolling BoringSSL from %s to %s...' % (old_head, new_head)

  # Look for commits with associated Chromium bugs.
  crbugs = set()
  crbug_commits = []
  update_note_commits = []
  log = Log(BORINGSSL_SRC_PATH, '%s..%s' % (old_head, new_head))
  for commit in log:
    has_bugs = False
    has_update_note = False
    for line in commit['message'].split('\n'):
      lower = line.lower()
      if lower.startswith('bug:') or lower.startswith('bug='):
        for bug in lower[4:].split(','):
          bug = bug.strip()
          if bug.startswith('chromium:'):
            crbugs.add(int(bug[len('chromium:'):]))
            has_bugs = True
      if lower.startswith('update-note:'):
        has_update_note = True
    if has_bugs:
      crbug_commits.append(commit)
    if has_update_note:
      update_note_commits.append(commit)

  UpdateDEPS(DEPS_PATH, old_head, new_head)

  # Checkout third_party/boringssl/src to generate new files.
  subprocess.check_call(['git', 'checkout', new_head], cwd=BORINGSSL_SRC_PATH)

  # Clear the old generated files.
  for (osname, arch, _, _, _) in generate_build_files.OS_ARCH_COMBOS:
    path = os.path.join(BORINGSSL_PATH, osname + '-' + arch)
    shutil.rmtree(path)
  for f in GENERATED_FILES:
    path = os.path.join(BORINGSSL_PATH, f)
    os.unlink(path)

  # Generate new ones.
  subprocess.check_call(['python',
                         os.path.join(BORINGSSL_SRC_PATH, 'util',
                                      'generate_build_files.py'),
                         '--embed_test_data=false',
                         'gn'],
                        cwd=BORINGSSL_PATH)

  # Commit everything.
  subprocess.check_call(['git', 'add', DEPS_PATH], cwd=SRC_PATH)
  for (osname, arch, _, _, _) in generate_build_files.OS_ARCH_COMBOS:
    path = os.path.join(BORINGSSL_PATH, osname + '-' + arch)
    subprocess.check_call(['git', 'add', path], cwd=SRC_PATH)
  for f in GENERATED_FILES:
    path = os.path.join(BORINGSSL_PATH, f)
    subprocess.check_call(['git', 'add', path], cwd=SRC_PATH)

  message = """Roll src/third_party/boringssl/src %s..%s

https://boringssl.googlesource.com/boringssl/+log/%s..%s

""" % (old_head[:9], new_head[:9], old_head, new_head)
  if crbug_commits:
    message += 'The following commits have Chromium bugs associated:\n'
    for commit in crbug_commits:
      message += '  ' + FormatCommit(commit)
    message += '\n'
  if update_note_commits:
    message += 'The following commits have update notes:\n'
    for commit in update_note_commits:
      message += '  ' + FormatCommit(commit)
    message += '\n'
  if crbugs:
    message += 'Bug: %s\n' % (', '.join(str(bug) for bug in sorted(crbugs)),)
  else:
    message += 'Bug: none\n'

  subprocess.check_call(['git', 'commit', '-m', message], cwd=SRC_PATH)

  # Print update notes.
  notes = subprocess.check_output(
      ['git', 'log', '--grep', '^Update-Note:', '-i',
       '%s..%s' % (old_head, new_head)], cwd=BORINGSSL_SRC_PATH).strip()
  if len(notes) > 0:
    print "\x1b[1mThe following changes contain updating notes\x1b[0m:\n\n"
    print notes

  return 0


if __name__ == '__main__':
  sys.exit(main())

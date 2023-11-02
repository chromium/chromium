#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import argparse
import logging
import mimetypes
import os
import re
import subprocess
import sys
import tempfile

from typing import List, Tuple

# Add third_party directory to the Python import path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import jinja2

# Oldest version of this directory that works for serving. Used to limit git
# history searches.
FIRST_REVISION = '08a37e09f110ab9cb2af3180f054f26a2fd274d6'
TEST_SUBDIR = 'webxr-samples'
INDEX_TEMPLATE = 'bucket_index.html'
LATEST_TEMPLATE = 'bucket_latest.html'

# Google Cloud storage bucket destination.
BUCKET = 'gs://chromium-webxr-test'

# URL templates used for creating the index page.
LINK_OMAHAPROXY = ('https://storage.googleapis.com/'
                   'chromium-find-releases-static/index.html#%s')
LINK_CRREV = 'https://crrev.com/%s'
LINK_CHROMIUMDASH = 'https://chromiumdash.appspot.com/commit/%s'

CR_POSITION_RE = re.compile(r'^Cr-Commit-Position:.*\#(\d+)')
BUCKET_COPY_RE = re.compile(r'^r(\d+)')

WINDOWS_EXES = {
  'git': 'git.bat',
  'gsutil.py': 'gsutil.py.bat',
}

SUFFIX_TYPES = {
  'js': 'application/javascript',
  'css': 'text/css',
  'html': 'text/html',
}

g_flags = None

def binary_name(cmd: str) -> str:
  """Finds the platform-appropriate name for an executable command."""

  # On Windows, the "subprocess" execution requires a name with extension.
  # Since we just need a couple of commands, use a simple replacement that
  # work for Chromium build environments. This isn't a general solution.
  if sys.platform != 'win32':
    return cmd
  if cmd in WINDOWS_EXES:
    return WINDOWS_EXES[cmd]
  raise Exeption('No known Windows executable for command "%s"' % cmd)

def run_command(*args: Tuple[str, ...]) -> str:
  """Runs a shell command and returns output. The output will be decoded,
  assuming utf-8 encoding and using strict error handling scheme."""
  platform_args = list(args)
  platform_args[0] = binary_name(platform_args[0])
  logging.debug('Executing: %s', platform_args)
  return subprocess.check_output(platform_args).decode('utf-8')

def run_readonly(*args) -> str:
  """Runs command expected to have no side effects, safe for dry runs."""
  return run_command(*args)

def run_modify(*args) -> str:
  """Runs command with side effects, skipped for dry runs."""
  if g_flags.dry_run:
    print('Dry-Run:', *args)
    return
  return run_command(*args)

def get_cr_positions() -> List[str]:
  """Retrieves list of Cr-Commit-Position entries for local commits"""
  revs = run_readonly('git', 'log', '--format=%H', FIRST_REVISION+'^..', '--',
                      TEST_SUBDIR)
  cr_positions = []
  for rev in revs.splitlines():
    cr_position = None
    msg = run_readonly('git', 'show', '-s', '--format=%B', rev)
    for line in msg.splitlines():
      m = CR_POSITION_RE.match(line)
      if m:
        cr_position = m.group(1)
    cr_positions.append(cr_position)
  return cr_positions

def get_commit_from_cr_position(position: str) -> str:
  """Given the Cr-Commit-Position, returns a string with commit hash.

  The function works on a best-effort basis. If commit hash cannot be found,
  it will return an empty string.

  Args:
    position: String containing Cr-Commit-Position number."""
  try:
    git_hash = run_readonly('git', 'crrev-parse', position).strip()
    logging.debug('`git crrev-parse %s` returned hash %s', position, git_hash)
    return git_hash
  except subprocess.CalledProcessError:
    logging.debug('`git crrev-parse %s` returned an error', position)
    return ''

def get_bucket_copies() -> List[str]:
  """Retrieves list of test subdirectories from Cloud Storage"""
  copies = []
  dirs = run_readonly('gsutil.py', 'ls', '-d', BUCKET)
  strip_len = len(BUCKET) + 1
  for rev in dirs.splitlines():
    pos = rev[strip_len:]
    m = BUCKET_COPY_RE.search(pos)
    if m:
      copies.append(m.group(1))
  return copies

def is_working_dir_clean() -> bool:
  """Checks if the git working directory has unsaved changes"""
  status = run_readonly('git', 'status', '--porcelain', '--untracked-files=no')
  return status.strip() == ''

def write_to_bucket(cr_position: str):
  """Copies the test directory to Cloud Storage"""
  destination = BUCKET + '/r' + cr_position
  run_modify('gsutil.py', '-m', 'rsync', '-x', 'media', '-r', './' + TEST_SUBDIR,
          destination)

  # The copy used mime types based on system-local mappings which may be
  # misconfigured. Sanity check and fix if needed.
  check_and_fix_content_types(destination)

def direct_publish_samples(source: str, dest_subfolder: str):
  destination = BUCKET + '/' + dest_subfolder
  run_modify('gsutil.py', '-m', 'rsync', '-x', 'media', '-r', './' + source,
          destination)

  # The copy used mime types based on system-local mappings which may be
  # misconfigured. Sanity check and fix if needed.
  check_and_fix_content_types(destination)

def check_and_fix_content_types(destination: str):
  mimetypes.init()
  for suffix, content_type in SUFFIX_TYPES.items():
    configured_type = mimetypes.types_map.get('.' + suffix)
    if configured_type != content_type:
      logging.info('Fixing content type mismatch for .%s: found %s, '
                   'expected %s.' % (suffix, configured_type, content_type))
      run_modify('gsutil.py', '-m', 'setmeta', '-h',
                 'Content-type:' + content_type,
                 destination + '/**.' + suffix)

def write_index():
  """Updates Cloud Storage index.html based on available test copies"""
  cr_positions = get_bucket_copies()
  cr_positions.sort(key=int, reverse=True)
  logging.debug('Index: %s', cr_positions)

  items = []

  for pos in cr_positions:
    rev = 'r' + pos
    links = []
    links.append({'href': '%s/index.html' % rev,
                  'anchor': 'index.html'})
    links.append({'href': LINK_CRREV % pos,
                  'anchor': '[crrev]'})
    links.append({'href': LINK_OMAHAPROXY % rev,
                  'anchor': '[find in releases]'})
    commit_hash = get_commit_from_cr_position(pos)
    if commit_hash:
      links.append({'href': LINK_CHROMIUMDASH % commit_hash,
                    'anchor' : '[chromium dash]'})
    items.append({'text': rev, 'links': links})

  template = jinja2.Template(open(INDEX_TEMPLATE).read())
  content = template.render({'items': items})
  logging.debug('index.html content:\n%s', content)

  with tempfile.NamedTemporaryFile(suffix='.html', delete=False) as temp:
    try:
      temp.write(content.encode('utf-8'))
      temp.seek(0)
      temp.close()
      run_modify('gsutil.py', 'cp', temp.name, BUCKET + '/index.html')
    finally:
      os.unlink(temp.name)

def write_latest():
  """Updates Cloud Storage latest.html based on available test copies, pointing
  to the latest copy."""
  cr_positions = get_bucket_copies()
  cr_positions.sort(key=int, reverse=True)
  logging.debug('Index: %s', cr_positions)

  if not cr_positions:
    logging.debug('No cr_positions found, skipping generation of latest.html')
    return

  logging.debug('Latest cr position: %s', cr_positions[0])

  template = jinja2.Template(open(LATEST_TEMPLATE).read())
  content = template.render({'revisionString' : cr_positions[0]})
  logging.debug('latest.html content:\n%s', content)

  with tempfile.NamedTemporaryFile(suffix='.html', delete=False) as temp:
    try:
      temp.write(content.encode('utf-8'))
      temp.seek(0)
      temp.close()
      run_modify('gsutil.py', 'cp', temp.name, BUCKET + '/latest.html')
    finally:
      os.unlink(temp.name)

def update_test_copies() -> bool:
  """Uploads a new test copy if available"""

  if not is_working_dir_clean() and not g_flags.ignore_unclean_status:
    raise Exception('Working directory is not clean, check "git status"')

  cr_positions = get_cr_positions()
  logging.debug('Found git commit positions: %s', cr_positions)
  if not cr_positions:
    raise Exception('No commit positions found')

  latest_cr_position = cr_positions[0]
  if latest_cr_position is None and not g_flags.force_destination_cr_position:
    raise Exception('Top commit has no Cr-Commit-Position. Sync to tip of tree?')

  existing_copies = get_bucket_copies()
  logging.debug('Found bucket copies: %s', existing_copies)

  out_cr_position = g_flags.force_destination_cr_position or latest_cr_position

  need_index_update = False
  if out_cr_position in existing_copies:
    logging.info('Destination "r%s" already exists, skipping write',
                 out_cr_position)
  else:
    write_to_bucket(out_cr_position)
    need_index_update = True

  return need_index_update

def numeric_string(value):
  """Ensure value is a string containing an integer."""
  # This is used for validating flag values - the usual idiom would be to use
  # "type=int", but that would convert the value to an integer, and it's more
  # convenient to keep it string-valued for use in concatenation elsewhere.
  # Throws an exception if the integer conversion fails.
  return str(int(value))

def main():
  parser = argparse.ArgumentParser(
      description="""
Copies the current '%s' content to a Google Cloud Storage bucket
subdirectory named after the Cr-Commit-Position, and writes an index.html file
linking to the known test directories, newest first.

The intended workflow is as follows:

- Create a CL that modifies the webxr_test_pages content. Ideally, if this
  reflects an incompatible WebXR API change, the code change and test change
  should be combined in the same CL.

- After review, submit the CL as usual.

- Rebase, or check out a fresh branch that includes the merged CL. The
  "git log" history of the webxr_test_pages should contain the merged CL and
  its Cr-Commit-Position, and there should be no local commits in the history.

- Run this script to upload the new test snapshot and update the index.html
  file to include it.

The script has sanity checks to confirm that the state is as expected. It will
not overwrite existing data, you need to manually remove bad or incomplete
content from failed uploads using the cloud console before retrying.
""" % TEST_SUBDIR,
      formatter_class=argparse.RawDescriptionHelpFormatter)

  parser.add_argument('-v', '--verbose', action="store_true",
                      help="Print debugging info")
  parser.add_argument('-n', '--dry-run', action="store_true",
                      help="Don't run state-changing commands")
  parser.add_argument('--update-index-only', action="store_true",
                      help=("Generate a new index.html file based on already "
                            "existing directories, ignoring local changes"))
  parser.add_argument('--ignore-unclean-status', action="store_true",
                      help=("Proceed with copy even if there are uncommitted "
                            "local changes in the git working directory"))
  parser.add_argument('--force-destination-cr-position', metavar='NUMBER',
                      type=numeric_string,
                      help=("Force writing current content to the specified "
                            "destination CR position instead of determining "
                            "the name based on local git history, bypassing "
                            "history sanity checks"))
  parser.add_argument('--bucket', default=BUCKET,
                      help=("Destination Cloud Storage location, including "
                            "'gs://' prefix"))
  parser.add_argument('--direct-publish-samples-source', default=None,
                      help=("Publish samples from this folder directly to a "
                            "bucket."))
  parser.add_argument('--direct-publish-samples-dest', default=None,
                      help=("Publish samples directly to this subfolder in the "
                            "chromium-webxr-samples bucket."))

  global g_flags
  g_flags = parser.parse_args()

  if g_flags.verbose:
    logging.basicConfig(level=logging.DEBUG)

  if not os.path.isdir(TEST_SUBDIR):
    raise Exception('Must be run from webxr_test_pages directory')

  node_modules = os.path.join(TEST_SUBDIR, 'js', 'cottontail', 'node_modules')
  if os.path.isdir(node_modules):
    raise Exception('Please delete the obsolete directory "%s"' % node_modules)

  if g_flags.direct_publish_samples_source and g_flags.direct_publish_samples_dest:
    direct_publish_samples(
      g_flags.direct_publish_samples_source,
      g_flags.direct_publish_samples_dest)
    return

  need_index_update = False
  if g_flags.update_index_only:
    need_index_update = True
  else:
    need_index_update = update_test_copies()

  # Create an index.html file covering all found test copies.
  if need_index_update:
    write_index()
    write_latest()
  else:
    logging.info('No changes, skipping index.html and latest.html update.')


if __name__ == '__main__':
  main()

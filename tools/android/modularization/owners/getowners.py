#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r'''Get chromium OWNERS information for android directories.

   tools/android/modularization/owners/getowners.py \
   --git-dir ~/chromium/src \
   -o ~/owners.json
'''

import argparse
import datetime
import functools
import logging
import multiprocessing
import os
import re
import time
from typing import Dict, Optional, Tuple

import owners_data
import owners_dir_metadata
import owners_exporter
import owners_git
import owners_input


def main():
  arg_parser = argparse.ArgumentParser(
      description='Traverses the chromium codebase gathering OWNERS data.')

  required_arg_group = arg_parser.add_argument_group('required arguments')
  required_arg_group.add_argument('--git-dir',
                                  required=True,
                                  help='Root directory to search for owners.')
  required_arg_group.add_argument('-o',
                                  '--output',
                                  required=True,
                                  help='File to write the result json to.')
  arg_parser.add_argument(
      '--limit-to-dir',
      help='Limit to a single directory. Used to restrict a smaller scope for '
      'debugging.')
  arg_parser.add_argument(
      '--dirmd-path',
      default='dirmd',
      help="Path to dirmd. If not specified, assume it's in PATH.")
  arg_parser.add_argument('--follow',
                          action='store_true',
                          help='Run git log with --follow to account for file '
                          'renames. Slightly more accurate but 9x slower.')
  arg_parser.add_argument('--no-cache',
                          action='store_true',
                          help='Avoids using the default cache dir.')
  arg_parser.add_argument('--cache-dir',
                          help='Defaults to git-dir/out/getowners_cache. Or if '
                          'a specific directory is passed then use that as the '
                          'cache dir.')
  arg_parser.add_argument('-v',
                          '--verbose',
                          action='store_true',
                          help='Used to display detailed logging.')
  arguments = arg_parser.parse_args()

  if arguments.verbose:
    level = logging.DEBUG
  else:
    level = logging.INFO
  logging.basicConfig(level=level,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  chromium_root = os.path.expanduser(arguments.git_dir)
  # Guarantee path does not end with '/'
  chromium_root = os.path.normpath(chromium_root)

  if arguments.no_cache:
    arguments.cache_dir = None
  else:
    if arguments.cache_dir is None:
      arguments.cache_dir = os.path.join(chromium_root, 'out',
                                         'getowners_cache')
    logging.info(f'Using cache dir: {arguments.cache_dir}')
    os.makedirs(arguments.cache_dir, exist_ok=True)

  logging.info(f'Finding android folders under {chromium_root}')
  paths_to_search = owners_input.get_android_folders(chromium_root,
                                                     arguments.limit_to_dir)

  logging.info(f'Reading dir metadata with {arguments.dirmd_path}.')
  all_dir_metadata = owners_dir_metadata.read_raw_dir_metadata(
      chromium_root, arguments.dirmd_path)

  logging.info(f'Processing {len(paths_to_search)} android folders.')
  with multiprocessing.Pool() as p:
    data = p.map(
        functools.partial(_process_requested_path, chromium_root,
                          all_dir_metadata, arguments.follow,
                          arguments.cache_dir), paths_to_search)

  logging.info(f'Writing data out to {arguments.output}')
  owners_exporter.to_json_file(data, arguments.output)
  logging.info(f'Completed.')


def _process_requested_path(
    chromium_root: str, all_dir_metadata: Dict, follow: bool,
    cache_dir: Optional[str], requested_path: owners_data.RequestedPath
) -> Tuple[owners_data.RequestedPath, owners_data.PathData]:
  '''Gets the necessary information from the git repository.'''
  start_time = time.time()
  owners_file = _find_owners_file(chromium_root, requested_path.path)
  owners = _build_owners_info(chromium_root, owners_file)
  git_data = _fetch_git_data(chromium_root, follow, cache_dir, requested_path)
  dir_metadata = owners_dir_metadata.build_dir_metadata(all_dir_metadata,
                                                        requested_path)
  path_data = owners_data.PathData(owners, git_data, dir_metadata)
  elapsed_time = time.time() - start_time
  logging.debug(f'Finished ({elapsed_time:4.1f}s) {requested_path}')
  return (requested_path, path_data)


def _fetch_git_data(chromium_root: str, follow: bool, cache_dir: Optional[str],
                    requested_path: owners_data.RequestedPath
                    ) -> owners_data.GitData:
  '''Fetches git data for a given directory for the last 182 days.

  Includes # of commits, reverts, relands, authors, and reviewers.
  '''

  line_delimiter = '\ncommit '
  author_search = r'^Author: (.*) <(.*)>'
  date_search = r'Date:   (.*)'
  reviewer_search = r'^    Reviewed-by: (.*) <(.*)>'
  revert_token = r'^    (\[?)Revert(\]?) \"'
  reland_token = r'^    (\[?)Reland(\]?) \"'

  ignored_authors = ('autoroll', 'roller')

  start_time = time.time()
  git_log = owners_git.get_log(chromium_root, requested_path.path, 182, follow,
                               cache_dir)
  elapsed_time = time.time() - start_time
  logging.debug(f'git log ({elapsed_time:4.1f}s) {requested_path}')
  git_data = owners_data.GitData()

  for commit_msg in git_log.split(line_delimiter):
    author_re = re.search(author_search, commit_msg,
                          re.IGNORECASE | re.MULTILINE)
    if author_re:
      author = author_re.group(2)
      if any(ignored in author for ignored in ignored_authors):
        continue  # ignore flagged authors
      git_data.authors[author] += 1

    reviewer_re = re.findall(reviewer_search, commit_msg,
                             re.IGNORECASE | re.MULTILINE)
    for _, reviewer in reviewer_re:
      git_data.reviewers[reviewer] += 1

    date_re = re.search(date_search, commit_msg, re.IGNORECASE | re.MULTILINE)
    if date_re and not git_data.latest_cl_date:
      d = date_re.group(1).strip().split(' ')[:-1]  # Minus tz offset.
      dobj = datetime.datetime.strptime(' '.join(d), '%a %b %d %H:%M:%S %Y')
      git_data.latest_cl_date = int(dobj.timestamp())

    git_data.cls += 1

    for i, line in enumerate(commit_msg.split('\n')):
      if i == 4:
        if re.search(revert_token, line, re.IGNORECASE | re.MULTILINE):
          git_data.reverted_cls += 1
        if re.search(reland_token, line, re.IGNORECASE | re.MULTILINE):
          git_data.relanded_cls += 1
        break

  git_data.lines_of_code = owners_git.get_total_lines_of_code(
      chromium_root, requested_path.path)
  git_data.number_of_files = owners_git.get_total_files(chromium_root,
                                                        requested_path.path)
  git_data.git_head = owners_git.get_head_hash(chromium_root)
  git_data.git_head_time = owners_git.get_last_commit_date(chromium_root)

  return git_data


def _find_owners_file(chromium_root: str, filepath: str) -> str:
  '''Returns the path to the OWNERS file for the given path (or up the tree).'''

  if not filepath.startswith(os.path.join(chromium_root, '')):
    filepath = os.path.join(chromium_root, filepath)

  if os.path.isdir(filepath):
    ofile = os.path.join(filepath, 'OWNERS')
  else:
    if 'OWNERS' in os.path.basename(filepath):
      ofile = filepath
    else:
      filepath = os.path.dirname(filepath)
      ofile = os.path.join(filepath, 'OWNERS')

  if os.path.exists(ofile):
    return ofile
  else:
    return _find_owners_file(chromium_root, os.path.dirname(filepath))


owners_map: Dict[str, owners_data.Owners] = {}


def _build_owners_info(chromium_root: str,
                       owners_filepath: str) -> owners_data.Owners:
  '''Creates a synthetic representation of an OWNERS file.'''

  if not owners_filepath: return None

  assert owners_filepath.startswith(os.path.join(chromium_root, ''))
  owners_file = owners_filepath[len(chromium_root) + 1:]
  if owners_file in owners_map:
    return owners_map[owners_file]

  owners = owners_data.Owners(owners_file)

  with open(owners_filepath, 'r') as f:
    for line in f:
      line = line.strip()
      if not line:
        continue
      elif line.startswith('file://'):
        owners.file_inherited = line[len('file://'):].strip()
      elif line.startswith('#'):
        continue
      elif line.startswith('per-file'):
        continue
      elif '@' in line:
        # Remove comments after the email
        owner_email = line.split(' ', 1)[0]
        owners.owners.append(line)

    owners_map[owners.owners_file] = owners

    _propagate_down_owner_variables(chromium_root, owners)

    return owners


def _propagate_down_owner_variables(chromium_root: str,
                                    owners: owners_data.Owners) -> None:
  '''For a given Owners, make sure that parent OWNERS are propagated down.

  Search in parent directories for OWNERS in case they do not exist
  in the current representation.
  '''

  parent_owners = owners
  visited = set()
  while parent_owners:
    if parent_owners.owners_file in visited:
      return
    if not owners.owners and parent_owners.owners:
      owners.owners.extend(parent_owners.owners)
    if owners.owners:
      return
    visited.add(parent_owners.owners_file)

    if parent_owners.file_inherited:
      parent_dir = parent_owners.file_inherited
    else:
      parent_dir = os.path.dirname(os.path.dirname(parent_owners.owners_file))
    parent_owners_file = _find_owners_file(chromium_root, parent_dir)
    parent_owners = _build_owners_info(chromium_root, parent_owners_file)


if __name__ == '__main__':
  main()

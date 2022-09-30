#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import subprocess
import pickle
import re
import os
from pathlib import PurePath
from os import path
from datetime import date, timedelta
from collections import namedtuple, defaultdict

Commit = namedtuple('Commit', ['hash', 'author', 'commit_date', 'dirs'])

# dict mapping each subdirectory and author to the number of their commits and
# modifications in that directory
DIRECTORY_AUTHORS = defaultdict(dict)

# cache for directory owners for memoisation purposes
OWNERS_CACHE = {}

# filename for pickle cache
CACHE_FILENAME = 'suggest_owners.cache'


def _RunGitCommand(options, cmd_args, pipe_output=False):
  repo_path = path.join(options.repo_path, '.git')
  cmd = ['git', '--git-dir', repo_path] + cmd_args
  print('>', ' '.join(cmd))
  if not pipe_output:
    return subprocess.check_output(cmd, encoding='utf-8')
  else:
    return subprocess.Popen(cmd, encoding='utf-8',
                            stdout=subprocess.PIPE).stdout


def _ValidAuthor(author):
  return author.endswith(
      ('@chromium.org', '@google.com')) and 'roller' not in author


# Returns additions/deletions by a commit to a directory (and its descendants).
def getEditsForDirectory(commit, directory):
  additions = deletions = 0
  for commit_directory, (directory_additions, directory_deletions) \
      in commit.dirs.items():
    # check if commit_directory is same as or a descendant of directory
    if isSubDirectory(directory, commit_directory):
      additions += directory_additions
      deletions += directory_deletions
  return additions, deletions


# This propagates a commit touching a directory to also be touching all
# ancesstor directories.
def _PropagateCommit(options, commit):
  touched_dirs = set()
  # first get all the touched dirs and their ancestors
  for directory in commit.dirs.keys():
    # PurePath.parent returns '.' for non absolute paths in the limit.
    while str(directory) != '.':
      touched_dirs.add(str(directory))
      # get the parent directory
      directory = PurePath(directory).parent
  # loop over them and calculate the edits per directory
  for directory in touched_dirs:
    author_commits, author_additions, author_deletions = \
        DIRECTORY_AUTHORS[directory].get(commit.author, (0,0,0))
    directory_additions, directory_deletions = \
        getEditsForDirectory(commit, directory)
    DIRECTORY_AUTHORS[directory][commit.author] = \
        (author_commits + 1, author_additions + directory_additions,
         author_deletions + directory_deletions)


# Checks if child_directory is same as or below parent_directory. For some
# reason the os.path module does not have this functionality.
def isSubDirectory(parent_directory, child_directory):
  parent_directory = PurePath(parent_directory)
  child_directory = PurePath(child_directory)
  return child_directory.is_relative_to(parent_directory)


def _GetGitLogCmd(options):
  # TODO(mheikal): git-log with --numstat vs --name-only takes 10x the time to
  # complete. It takes >15 mins for git log --numstat to return the 1 year git
  # history of the full repo. Should probably add a script flag to switch off
  # keeping track of number of modifications per commit.
  date_limit = date.today() - timedelta(days=options.days_ago)
  format_string = "%h,%ae,%cI"
  cmd_args = [
    'log',
    '--since', date_limit.isoformat(),
    '--numstat',
    '--pretty=format:%s'%format_string,
  ]
  # has to be last arg
  if options.subdirectory:
    cmd_args += ['--', options.subdirectory]
  return cmd_args


def _ParseCommitLine(line):
  commit_hash, author, commit_date = line.split(",")
  return Commit(hash=commit_hash, author=author, commit_date=commit_date,
                dirs={})


def _ParseFileStatsLine(current_commit, line):
  try:
    additions, deletions, filepath = line.split('\t')
  except ValueError:
    return False
  if additions == '-':
    additions = 0
  else:
    additions = int(additions)
  if deletions == '-':
    deletions = 0
  else:
    deletions = int(deletions)
  if additions == 0 and deletions == 0:
    return True
  dir_path = path.dirname(filepath)
  # For git renames, we count the destination directory
  if '=>' in dir_path:
    dir_path = re.sub(r'\{[^=]* => ([^\}]*)\}', r'\1', dir_path)
    # remove possibly empty path parts.
    dir_path = dir_path.replace('//', '/')
  commit_additions, commit_deletions = \
      current_commit.dirs.get(dir_path, (0,0))
  current_commit.dirs[dir_path] = (
      additions + commit_additions, deletions + commit_deletions)
  return True


def processAllCommits(options):
  if not options.subdirectory and options.days_ago > 100:
    print('git log for your query might take > 5 minutes, limit by a '
          'subdirectory or reduce the number of days of history to low double '
          'digits to make this faster. There is no progress indicator, it is '
          'all waiting for single git log to finish.')
  output_pipe = _RunGitCommand(options,
                               _GetGitLogCmd(options),
                               pipe_output=True)
  current_commit = None
  for line in iter(output_pipe.readline, ''):
    line = line.rstrip('\n')
    if current_commit is None:
      current_commit = _ParseCommitLine(line)
    else:
      if line == '': # all commit details read
        if _ValidAuthor(current_commit.author):
          _PropagateCommit(options, current_commit)
        current_commit = None
      else:
        # Merge commits weird out git-log. If we fail to parse the line, then
        # the last commit was a merge and this line is actually another commit
        # description line.
        if not _ParseFileStatsLine(current_commit, line):
          current_commit = _ParseCommitLine(line)
  # process the final commit
  if _ValidAuthor(current_commit.author):
    _PropagateCommit(options, current_commit)
  print('Done parsing commit log.')


def _CountCommits(directory):
  return sum(
      [count for (count, _a, _d) in DIRECTORY_AUTHORS[directory].values()])


def _GetOwnerLevel(options, author, directory):
  sorted_owners = sorted(_GetOwners(options, directory), key=lambda e: e[1])
  for owner, level in sorted_owners:
    if author == owner:
      return level
  else:
    return -1


# Returns the owners for a repo subdirectory. This does not understand per-file
# directives.
# TODO(mheikal): use depot_tools owners.py for parsing owners files.
def _GetOwners(options, directory_path):
  if directory_path in OWNERS_CACHE:
    return OWNERS_CACHE[directory_path]
  owners_path = path.join(options.repo_path, directory_path, 'OWNERS')
  owners = set()
  parent_dir = directory_path
  owner_level = 0
  while parent_dir != '':
    if path.isfile(owners_path):
      parsed_owners, noparent = _ParseOwnersFile(options, owners_path)
      owners.update([(owner, owner_level) for owner in parsed_owners])
      owner_level += 1
      if noparent:
        break
    parent_dir = path.dirname(parent_dir)
    owners_path = path.join(parent_dir, 'OWNERS')
  OWNERS_CACHE[directory_path] = set(owners)
  return owners


# Parse an OWNERS file, returns set of owners and if the file sets noparent
def _ParseOwnersFile(options, filepath):
  owners = set()
  noparent = False
  with open(filepath) as f:
    for line in f.readlines():
      line = line.strip()
      # The script deals with directories so per-files are ignored.
      if line == '' or line[0] == '#' or line.startswith('per-file'):
        continue
      if line.startswith('file://'):
        relpath = line[7:]
        abspath = path.join(options.repo_path, relpath)
        parsed_owners, _ = _ParseOwnersFile(options, abspath)
        owners.update(parsed_owners)
      if line == 'set noparent':
        noparent = True
      index = line.find('@chromium.org')
      if index > -1:
        owners.add(line[:index + len('@chromium.org')])
  return owners, noparent


# Trivial directories are ones that just contain a single child subdir and
# nothing else.
def _IsTrivialDirectory(options, repo_subdir):
  try:
    return len(os.listdir(path.join(options.repo_path, repo_subdir))) == 1
  except OSError:
    # directory no longer exists
    return False


def computeSuggestions(options):
  directory_suggestions = []
  for directory, authors in sorted(DIRECTORY_AUTHORS.items()):
    if _IsTrivialDirectory(options, directory):
      continue
    if _CountCommits(directory) < options.dir_commit_limit:
      continue
    # skip suggestions for directories outside the passed in directory
    if (options.subdirectory
        and not isSubDirectory(options.subdirectory, directory)):
      continue
    # sort authors by descending number of commits
    sorted_authors = sorted(authors.items(), key=lambda entry: -entry[1][0])
    # keep only authors above the limit
    suggestions = [(a,c) for a,c in sorted_authors if \
                   a not in options.ignore_authors \
                   and c[0] >= options.author_cl_limit]
    directory_suggestions.append((directory, suggestions))
  return directory_suggestions


def _PrintSettings(options):
  print('Showing directories with at least ({}) commits in the last ({}) '
        'days.'.format(options.dir_commit_limit, options.days_ago))
  print('Showing top ({}) committers who have commited at least ({}) commits '
        'to the directory in the last ({}) days.'.format(
            options.max_suggestions, options.author_cl_limit,
            options.days_ago))
  print('(owners+N) represents distance through OWNERS files for said owner\n')


def printSuggestions(options, directory_suggestions):
  print('\nCommit stats:')
  _PrintSettings(options)
  for directory, suggestions in directory_suggestions:
    print('{}: {} commits in the last {} days'.format(
        directory, _CountCommits(directory), options.days_ago))
    non_owner_suggestions = 0
    for author, (commit_count, additions, deletions) in suggestions:
      owner_level = _GetOwnerLevel(options, author, directory)
      if owner_level > -1:
        owner_string = ' (owner+{})'.format(owner_level)
      else:
        non_owner_suggestions +=1
        owner_string = ''
      print('{}{}, commits: {}, additions:{}, deletions: {}'.format(
          author, owner_string, commit_count, additions, deletions))
      if non_owner_suggestions >= options.max_suggestions:
        break
    print()


def _GetHeadCommitHash(options):
  return _RunGitCommand(options, ['rev-parse', 'HEAD']).strip()


def _GetCacheMetadata(options):
  return _GetHeadCommitHash(options), options.days_ago, options.subdirectory


def _IsCacheValid(options, metadata):
  head_hash, days_ago, cached_subdirectory = metadata
  if head_hash != _GetHeadCommitHash(options):
    return False
  if days_ago != options.days_ago:
    return False
  if (cached_subdirectory is not None
      and not isSubDirectory(cached_subdirectory, options.subdirectory)):
    return False
  return True


def cacheProcessedCommits(options):
  metadata = _GetCacheMetadata(options)
  with open(CACHE_FILENAME, 'wb') as f:
    pickle.dump((metadata, DIRECTORY_AUTHORS), f)


def maybeRestoreProcessedCommits(options):
  global DIRECTORY_AUTHORS
  if not path.exists(CACHE_FILENAME):
    return False
  with open(CACHE_FILENAME, 'rb') as f:
    stored_metadata, cached_directory_authors = pickle.load(f)
    if _IsCacheValid(options, stored_metadata):
      print('Loading from cache')
      DIRECTORY_AUTHORS = cached_directory_authors
      return True
    else:
      print('Cache is stale or invalid, must rerun `git log`')
      return False

def do(options):
  if options.skip_cache or not maybeRestoreProcessedCommits(options):
    processAllCommits(options)
    cacheProcessedCommits(options)
  directory_suggestions = computeSuggestions(options)
  printSuggestions(options, directory_suggestions)


def main():
  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('repo_path')
  parser.add_argument('--days-ago', type=int,
                      help='Number of days of history to search through.',
                      default=365, metavar='DAYS_AGO')
  parser.add_argument('--subdirectory',
                      help='Limit suggestions to this subdirectory', default='')
  parser.add_argument('--ignore-authors',
                      help='Ignore this comma separated list of authors')
  parser.add_argument('--max-suggestions', type=int, help='Maximum number of '
                      'suggested authors per directory.', default=5)
  parser.add_argument('--author-cl-limit', type=int, help='Do not suggest '
                      'authors who have commited less than this to the '
                      'directory in the last DAYS_AGO days.', default=10)
  parser.add_argument('--dir-commit-limit', type=int, help='Skip directories '
                      'with less than this number of commits in the last '
                      'DAYS_AGO days.', default=100)
  parser.add_argument('--skip-cache', action='store_true',
                      help='Do not read from cache.', default=False)
  options = parser.parse_args()
  if options.ignore_authors:
    options.ignore_authors = set(
        map(str.strip, options.ignore_authors.split(',')))
  else:
    options.ignore_authors = set()
  do(options)


if __name__ == '__main__':
  main()

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import re
import sys
import shutil
import logging

import utils.command_util as command
import utils.constants as const
from utils.command_error import CommandError


_DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.basename(__file__), '../../..'))
_COMMON_EXTENSIONS = ('.java', '.cc', '.mm', '.h', '.json')


def _CodeSearchFiles(query_args: list[str]) -> list[str]:
  lines: list[str] = command.RunCommand([
      'cs',
      '-l',
      # Give the local path to the file, if the file exists.
      '--local',
      # Restrict our search to Chromium
      'git:chrome-internal/codesearch/chrome/src@main',
  ] + query_args).splitlines()
  return [l.strip() for l in lines if l.strip()]


def _FindRemoteCandidates(target: str) -> tuple[list[str], list[str]]:
  """Find files using a remote code search utility, if installed."""
  if not shutil.which('cs'):
    return [], []
  results: list[str] = _CodeSearchFiles([f'file:{target}'])
  exact: set[str] = set()
  close: set[str] = set()
  for filename in results:
    file_validity: const.TestValidity = IsTestFile(filename)
    if file_validity is const.TestValidity.VALID_TEST:
      exact.add(filename)
    elif file_validity is const.TestValidity.MAYBE_A_TEST:
      close.add(filename)
  return list(exact), list(close)


def IsTestFile(file_path: str) -> const.TestValidity:
  if not const.TEST_FILE_NAME_REGEX.match(file_path):
    return const.TestValidity.NOT_A_TEST

  # Verify the file contains actual test definitions.
  try:
    if file_path.endswith(('.cc', '.mm')):
      content = pathlib.Path(file_path).read_text(encoding='utf-8')
      if const.GTEST_TEST_DEFINITION_MACRO_REGEX.search(content) is not None:
        return const.TestValidity.VALID_TEST
    elif file_path.endswith('.java'):
      content = pathlib.Path(file_path).read_text(encoding='utf-8')
      if const.JUNIT_TEST_ANNOTATION_REGEX.search(content) is not None:
        return const.TestValidity.VALID_TEST
  except IOError:
    pass

  # It may still be a test file, even if it doesn't include a test definition.
  return const.TestValidity.MAYBE_A_TEST


def _RecursiveMatchFilename(folder: str,
                            filename: str) -> tuple[list[str], list[str]]:
  current_dir: str = os.path.split(folder)[-1]
  if current_dir.startswith('out') or current_dir.startswith('.'):
    return ([], [])
  exact: list[str] = []
  close: list[str] = []
  try:
    with os.scandir(folder) as it:
      for entry in it:
        if (entry.is_symlink()):
          continue
        if (entry.is_file() and filename in entry.path
            and not os.path.basename(entry.path).startswith('.')):
          file_validity: const.TestValidity = IsTestFile(entry.path)
          if file_validity is const.TestValidity.VALID_TEST:
            exact.append(entry.path)
          elif file_validity is const.TestValidity.MAYBE_A_TEST:
            close.append(entry.path)
        if entry.is_dir():
          # On Windows, junctions are like a symlink that python interprets as a
          # directory, leading to exceptions being thrown. We can just catch and
          # ignore these exceptions like we would ignore symlinks.
          try:
            matches: tuple[list[str], list[str]] = _RecursiveMatchFilename(
                entry.path, filename)
            exact += matches[0]
            close += matches[1]
          except FileNotFoundError:
            logging.debug(f'Failed to scan directory "{entry}" - junction?')
            pass
  except PermissionError:
    logging.warning(f'Permission error while scanning {folder}')

  return (exact, close)


def _FindTestFilesInDirectory(directory: str) -> list[str]:
  test_files: list[str] = []
  logging.debug('Test files:')
  for root, _, files in os.walk(directory):
    for f in files:
      path: str = os.path.join(root, f)
      file_validity: const.TestValidity = IsTestFile(path)
      if file_validity is const.TestValidity.VALID_TEST:
        logging.debug(path)
        test_files.append(path)
      elif file_validity is const.TestValidity.MAYBE_A_TEST:
        logging.debug(path +
                      ' matched but doesn\'t include gtest files, skipping.')
  return test_files


def SearchForTestsByName(terms: list[str], quiet: bool,
                         remote_search: bool) -> tuple[list[str], str]:

  def GetPatternForTerm(term: str) -> str:
    ANY: str = '.' if not remote_search else r'[\s\S]'
    slash_parts: list[str] = term.split('/')
    # These are the formats, for now, just ignore the prefix and suffix here.
    # Prefix/Test.Name/Suffix  -> \bTest\b.*\bName\b
    # Test.Name/Suffix         -> \bTest\b.*\bName\b
    # Test.Name                -> \bTest\b.*\bName\b
    if len(slash_parts) <= 2:
      dot_parts = slash_parts[0].split('.')
    else:
      dot_parts = slash_parts[1].split('.')
    return f'{ANY}*'.join(r'\b' + re.escape(p) + r'\b' for p in dot_parts)

  def GetFilterForTerm(term: str) -> str:
    # If the user supplied a '/', assume they've included the full test name.
    if '/' in term:
      return term
    # If there's no '.', assume this is a test prefix or suffix.
    if '.' not in term:
      return '*' + term + '*'
    # Otherwise run any parameterized tests with this prefix.
    return f'{term}:{term}/*'

  pattern: str = '|'.join(f'({GetPatternForTerm(t)})' for t in terms)

  # find files containing the tests.
  if not remote_search:
    # Use ripgrep.
    try:
      files = [
          f for f in command.RunCommand([
              'rg',
              '-l',
              '--multiline',
              '--multiline-dotall',
              '-g',
              const.GTEST_FILE_NAME_GLOB,
              '-g',
              const.PREF_MAPPING_FILE_NAME_GLOB,
              '--glob-case-insensitive',
              pattern,
              str(const.SRC_DIR),
          ]).splitlines()
      ]
    except CommandError as e:
      if e.return_code == 1:
        # Exit status 1: no matches found.
        files = []
      else:
        # Exit status 2: error (regex syntax error, unable to read file).
        raise
    logging.debug(f'rg found: {files}')
  else:
    # Use code search.
    files = _CodeSearchFiles(['pcre:true', pattern])
  files = [f for f in files if IsTestFile(f) != const.TestValidity.NOT_A_TEST]
  gtest_filter: str = ':'.join(GetFilterForTerm(t) for t in terms)

  if files and not quiet:
    logging.info('Found tests in files:')
    limit = 50
    for f in files[:limit]:
      logging.info(f'  {f}')
    if len(files) > limit:
      logging.info(f'... ({len(files)} total)')
  return files, gtest_filter


def IsProbablyFile(name: str) -> bool:
  # Returns whether the name is likely a test file name, path,
  # or directory path.
  return (name.endswith(_COMMON_EXTENSIONS) or os.path.exists(name)
          or os.path.exists(os.path.join(_DIR_SOURCE_ROOT, name)))


def _FindTestForFile(target: os.PathLike[str]) -> str | None:
  root, ext = os.path.splitext(target)
  # If the target is a C++ implementation file, try to guess the test file.
  # Candidates should be ordered most to least promising.
  test_candidates: list[str] = [target]
  if ext == '.h':
    # `*_unittest.{cc,mm}` are both possible.
    test_candidates.append(f'{root}_unittest.cc')
    test_candidates.append(f'{root}_unittest.mm')
  elif ext == '.cc' or ext == '.mm':
    test_candidates.append(f'{root}_unittest{ext}')
  else:
    return str(target)

  maybe_valid: list[str] = []
  for candidate in test_candidates:
    if not os.path.isfile(candidate):
      continue
    validity: const.TestValidity = IsTestFile(str(candidate))
    if validity is const.TestValidity.VALID_TEST:
      return str(candidate)
    elif validity is const.TestValidity.MAYBE_A_TEST:
      maybe_valid.append(str(candidate))
  return maybe_valid[0] if maybe_valid else None


def FindMatchingTestFiles(target: str,
                          remote_search: bool = False,
                          path_index: int | None = None) -> list[str]:
  # Return early if there's an exact file match.
  exists = os.path.isfile(target)
  if not exists:
    src_rel_path = os.path.join(_DIR_SOURCE_ROOT, target)
    exists = os.path.isfile(src_rel_path)
    if exists:
      target = src_rel_path
  if exists:
    if test_file := _FindTestForFile(target):
      return [test_file]
    command.ExitWithMessage(f"{target} doesn't look like a test file")
  # If this is a directory, return all the test files it contains.
  if os.path.isdir(target):
    files: list[str] = _FindTestFilesInDirectory(target)
    if not files:
      command.ExitWithMessage('No tests found in directory')
    return files

  if sys.platform.startswith('win32') and os.path.altsep in target:
    # Use backslash as the path separator on Windows to match os.scandir().
    logging.debug('Replacing ' + os.path.altsep + ' with ' + os.path.sep +
                  ' in: ' + target)
    target = target.replace(os.path.altsep, os.path.sep)
  logging.debug('Finding files with full path containing: ' + target)

  if remote_search:
    exact, close = _FindRemoteCandidates(target)
    if not exact and not close:
      logging.info('Failed to find remote candidates; searching recursively')
      exact, close = _RecursiveMatchFilename(str(const.SRC_DIR), target)
  else:
    logging.warning(f'Doing a slow local search for {target}. '
                    f'Consider installing `cs` or using -r.')
    exact, close = _RecursiveMatchFilename(str(const.SRC_DIR), target)

  if exact:
    logging.debug('Found exact matching file(s):')
    logging.debug('\n'.join(exact))
  if close:
    logging.debug('Found possible matching file(s):')
    logging.debug('\n'.join(close))

  if len(exact) >= 1:
    # Given "Foo", don't ask to disambiguate ModFoo.java vs Foo.java.
    more_exact: list[str] = [
        p for p in exact if os.path.basename(p) in (target, f'{target}.java')
    ]
    if len(more_exact) == 1:
      test_files = more_exact
    else:
      test_files = exact
  else:
    test_files = close

  if len(test_files) > 1:
    if path_index is not None and 0 <= path_index < len(test_files):
      test_files = [test_files[path_index]]
    else:
      test_files = [command.HaveUserPickFile(test_files)]
  if not test_files:
    command.ExitWithMessage(f'Target "{target}" did not match any files.')
  return test_files


def _GetChangedFiles() -> list[str]:
  # Find files updated in both committed and uncommitted changes.
  merge_base_command: list[str] = ['git', 'merge-base', 'origin/main', 'HEAD']
  merge_base: str = command.RunCommand(merge_base_command).strip()
  git_command: list[str] = [
      'git', 'diff', '--name-only', '--diff-filter=ACMRT', merge_base
  ]
  changed_files: list[str] = command.RunCommand(git_command).splitlines()
  return changed_files


def GetChangedTestFiles() -> list[str]:
  """Gets test files modified in git."""
  changed_files = _GetChangedFiles()
  test_files: list[str] = []

  for f in changed_files:
    if IsTestFile(f) is not const.TestValidity.NOT_A_TEST:
      test_files.append(f)
  return test_files


def _GetPotentiallyRelatedTestFiles(basename: str, test_exts: list[str],
                                    remote_search: bool) -> list[str]:
  """Locate all test files starting with basename."""
  if remote_search:
    ext_pattern = '|'.join(e.strip('.') for e in test_exts)
    cs_pattern = f'file:(^|/){basename}[^/]*\.({ext_pattern})$'
    return _CodeSearchFiles([cs_pattern, 'pcre:true'])

  rg_command: list[str] = ['rg', '--files']
  for test_ext in test_exts:
    rg_command.extend(['-g', f'{basename}*{test_ext}'])
  rg_command.append(str(const.SRC_DIR))

  try:
    return command.RunCommand(rg_command).splitlines()
  except CommandError as e:
    if e.return_code == 1:
      return []
    raise


def _CheckIfFileExists(basename: str, check_exts: list[str],
                       remote_search: bool) -> bool:
  """Checks existence with the given stem and any of the extensions."""
  if remote_search:
    check_ext_pattern = '|'.join(e.strip('.') for e in check_exts)
    cs_check_pattern = f'file:(^|/){basename}\\.({check_ext_pattern})$'
    return bool(_CodeSearchFiles([cs_check_pattern, 'pcre:true']))

  rg_cmd = ['rg', '--files']
  for check_ext in check_exts:
    rg_cmd.extend(['-g', f'{basename}{check_ext}'])
  rg_cmd.append(str(const.SRC_DIR))

  try:
    return bool(command.RunCommand(rg_cmd).strip())
  except CommandError:
    return False


def _FindRelatedTestFiles(impl_path: str,
                          remote_search: bool = False) -> list[str]:
  """Finds test files related to an implementation file."""
  # Uses iterative suffix removal as a heuristic.
  _, filename = os.path.split(impl_path)
  basename, ext = os.path.splitext(filename)

  match ext:
    case '.cc' | '.h' | '.mm':
      test_exts = ['.cc', '.mm']
      check_exts = ['.cc', '.h', '.mm']
      split_pattern = r'_'  # snake_case
      separator = '_'
    case '.java':
      test_exts = ['.java']
      check_exts = ['.java']
      split_pattern = r'(?=[A-Z])'  # PascalCase
      separator = ''
    case _:
      # Only C++ and Java code files are supported.
      return []

  candidates = _GetPotentiallyRelatedTestFiles(basename, test_exts,
                                               remote_search)

  def generate_stems(name: str):
    # Generator that iteratively yields the name
    # with the last token peeled off.
    parts = [p for p in re.split(split_pattern, name) if p]
    while len(parts) > 1:
      parts.pop()
      yield separator.join(parts)

  related_tests: list[str] = []
  exists_cache: dict[str, bool] = {}

  for candidate in candidates:
    if IsTestFile(candidate) == const.TestValidity.NOT_A_TEST:
      continue

    cand_basename = pathlib.Path(candidate).stem

    # Iteratively peel back one token at a time from the end of the filename.
    # E.g. foo_manager_browser_test.cc would be peeled like:
    # foo_manager_browser_test -> foo_manager_browser -> foo_manager -> foo
    for current_stem in generate_stems(cand_basename):

      # If we peeled back enough to match the original modified file exactly
      # this test file is said to be related to the original target file.
      if current_stem == basename:
        related_tests.append(candidate)
        break

      # Otherwise, check if this intermediate stem exists as its own
      # implementation file.
      if current_stem not in exists_cache:
        exists_cache[current_stem] = _CheckIfFileExists(current_stem,
                                                        check_exts,
                                                        remote_search)

      # If this intermediate stem exists this test is not considered
      # related the original target file.
      if exists_cache[current_stem]:
        break

  return related_tests


def GetRelatedTestFiles(remote_search: bool = False) -> list[str]:
  """Gets files modified in git, and finds their related test files."""
  changed_files = _GetChangedFiles()
  related_test_files: set[str] = set()

  if shutil.which('cs') is None:
    remote_search = False

  for f in changed_files:
    # If the modified file is ALREADY a test file, just add it directly.
    if IsTestFile(f) is const.TestValidity.VALID_TEST:
      related_test_files.add(f)
    else:
      # If it's an implementation file, find the tests related to it.
      related_test_files.update(
          _FindRelatedTestFiles(f, remote_search=remote_search))

  return sorted(related_test_files)

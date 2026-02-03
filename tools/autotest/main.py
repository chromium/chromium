# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs a test by filename.

This script finds the appropriate test suites for the specified test files,
directories, or test names, builds it, then runs it with the (optionally)
specified filter, passing any extra args on to the test runner.

Examples:
# Run the test target for bit_cast_unittest.cc. Use a custom test filter instead
# of the automatically generated one.
autotest.py -C out/Desktop bit_cast_unittest.cc --gtest_filter=BitCastTest*

# Find and run UrlUtilitiesUnitTest.java's tests, pass remaining parameters to
# the test binary.
autotest.py -C out/Android UrlUtilitiesUnitTest --fast-local-dev -v

# Run all tests under base/strings.
autotest.py -C out/foo --run-all base/strings

# Run tests in multiple files or directories.
autotest.py -C out/foo base/strings base/pickle_unittest.cc

# Run only the test on line 11. Useful when running autotest.py from your text
# editor.
autotest.py -C out/foo --line 11 base/strings/strcat_unittest.cc

# Search for and run tests with the given names.
autotest.py -C out/foo StringUtilTest.IsStringUTF8 SpanTest.AsStringView
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import shutil

import utils.command_util as command
import utils.constants as const
import utils.telemetry as telemetry

from pathlib import Path

sys.path.append(str(const.SRC_DIR / 'build'))
import gn_helpers

sys.path.append(str(const.SRC_DIR / 'build' / 'android'))
from pylib import constants


def CodeSearchFiles(query_args: list[str]) -> list[str]:
  lines: list[str] = command.RunCommand([
      'cs',
      '-l',
      # Give the local path to the file, if the file exists.
      '--local',
      # Restrict our search to Chromium
      'git:chrome-internal/codesearch/chrome/src@main',
  ] + query_args).splitlines()
  return [l.strip() for l in lines if l.strip()]


def FindRemoteCandidates(target: str) -> tuple[list[str], list[str]]:
  """Find files using a remote code search utility, if installed."""
  if not shutil.which('cs'):
    return [], []
  results: list[str] = CodeSearchFiles([f'file:{target}'])
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
  if file_path.endswith('.cc') or file_path.endswith('.mm'):
    # Try a bit harder to remove non-test files for c++. Without this,
    # 'autotest.py base/' finds non-test files.
    try:
      with open(file_path, 'r', encoding='utf-8') as f:
        if const.GTEST_INCLUDE_REGEX.search(f.read()) is not None:
          return const.TestValidity.VALID_TEST
    except IOError:
      pass
    # It may still be a test file, even if it doesn't include a gtest file.
    return const.TestValidity.MAYBE_A_TEST
  return const.TestValidity.VALID_TEST


@telemetry.tracer.start_as_current_span('chromium.tools.autotest.build')
def BuildTestTargets(out_dir: str, targets: list[str], dry_run: bool,
                     quiet: bool, is_retry: bool) -> bool:
  """Builds the specified targets with ninja"""
  cmd: list[str] = gn_helpers.CreateBuildCommand(out_dir) + targets
  print('Building: ' + shlex.join(cmd))
  if (dry_run):
    return True

  completed_process: subprocess.CompletedProcess[str] = subprocess.run(
      cmd, capture_output=quiet, encoding='utf-8')

  telemetry.RecordBuildAttributes(is_retry, completed_process.returncode == 0)

  if completed_process.returncode != 0:
    if quiet:
      before, _, after = completed_process.stdout.partition('stderr:')
      if not after:
        before, _, after = completed_process.stdout.partition('stdout:')
      if after:
        print(after)
      else:
        print(before)
    return False
  return True


def RecursiveMatchFilename(folder: str,
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
            matches: tuple[list[str], list[str]] = RecursiveMatchFilename(
                entry.path, filename)
            exact += matches[0]
            close += matches[1]
          except FileNotFoundError:
            if const.DEBUG:
              print(f'Failed to scan directory "{entry}" - junction?')
            pass
  except PermissionError:
    print(f'Permission error while scanning {folder}')

  return (exact, close)


def FindTestFilesInDirectory(directory: str) -> list[str]:
  test_files: list[str] = []
  if const.DEBUG:
    print('Test files:')
  for root, _, files in os.walk(directory):
    for f in files:
      path: str = os.path.join(root, f)
      file_validity: const.TestValidity = IsTestFile(path)
      if file_validity is const.TestValidity.VALID_TEST:
        if const.DEBUG:
          print(path)
        test_files.append(path)
      elif const.DEBUG and file_validity is const.TestValidity.MAYBE_A_TEST:
        print(path + ' matched but doesn\'t include gtest files, skipping.')
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
    files = [
        f for f in command.RunCommand([
            'rg',
            '-l',
            '--multiline',
            '--multiline-dotall',
            '-t',
            'cpp',
            '-t',
            'java',
            '-t',
            'objcpp',
            pattern,
            str(const.SRC_DIR),
        ]).splitlines()
    ]
  else:
    # Use code search.
    files = CodeSearchFiles(['pcre:true', pattern])
  files = [f for f in files if IsTestFile(f) != const.TestValidity.NOT_A_TEST]
  gtest_filter: str = ':'.join(GetFilterForTerm(t) for t in terms)

  if files and not quiet:
    print('Found tests in files:')
    print('\n'.join([f'  {f}' for f in files]))
  return files, gtest_filter


def IsProbablyFile(name: str) -> bool:
  '''Returns whether the name is likely a test file name, path, or directory path.'''
  return bool(const.TEST_FILE_NAME_REGEX.match(name)) or os.path.exists(name)


def FindMatchingTestFiles(target: str,
                          remote_search: bool = False,
                          path_index: int | None = None) -> list[str]:
  # Return early if there's an exact file match.
  if os.path.isfile(target):
    if test_file := _FindTestForFile(target):
      return [test_file]
    command.ExitWithMessage(f"{target} doesn't look like a test file")
  # If this is a directory, return all the test files it contains.
  if os.path.isdir(target):
    files: list[str] = FindTestFilesInDirectory(target)
    if not files:
      command.ExitWithMessage('No tests found in directory')
    return files

  if sys.platform.startswith('win32') and os.path.altsep in target:
    # Use backslash as the path separator on Windows to match os.scandir().
    if const.DEBUG:
      print('Replacing ' + os.path.altsep + ' with ' + os.path.sep + ' in: ' +
            target)
    target = target.replace(os.path.altsep, os.path.sep)
  if const.DEBUG:
    print('Finding files with full path containing: ' + target)

  if remote_search:
    exact, close = FindRemoteCandidates(target)
    if not exact and not close:
      print('Failed to find remote candidates; searching recursively')
      exact, close = RecursiveMatchFilename(str(const.SRC_DIR), target)
  else:
    exact, close = RecursiveMatchFilename(str(const.SRC_DIR), target)

  if const.DEBUG:
    if exact:
      print('Found exact matching file(s):')
      print('\n'.join(exact))
    if close:
      print('Found possible matching file(s):')
      print('\n'.join(close))

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


# A persistent cache to avoid running gn on repeated runs of autotest.
class TargetCache:

  def __init__(self, out_dir: str) -> None:
    self.out_dir = out_dir
    self.path: str = os.path.join(out_dir, 'autotest_cache')
    self.gold_mtime: float = self.GetBuildNinjaMtime()
    self.cache: dict[str, list[str]] = {}

    if not os.path.exists(self.path):
      return

    try:
      with open(self.path, 'r') as f:
        mtime, cache = json.load(f)
      if mtime == self.gold_mtime:
        self.cache = cache
    except (json.JSONDecodeError, ValueError, OSError):
      pass

  def Save(self) -> None:
    with open(self.path, 'w') as f:
      json.dump([self.gold_mtime, self.cache], f)

  def Find(self, test_paths: list[str]) -> list[str] | None:
    key: str = ' '.join(test_paths)
    return self.cache.get(key, None)

  def Store(self, test_paths: list[str], test_targets: list[str]) -> None:
    key: str = ' '.join(test_paths)
    self.cache[key] = test_targets

  def GetBuildNinjaMtime(self) -> float:
    return os.path.getmtime(os.path.join(self.out_dir, 'build.ninja'))

  def IsStillValid(self) -> bool:
    return self.GetBuildNinjaMtime() == self.gold_mtime


def _TestTargetsFromGnRefs(targets: list[str]) -> list[str]:
  # Prevent repeated targets.
  all_test_targets: set[str] = set()

  # Find "standard" targets (e.g., GTests).
  standard_targets: list[str] = [t for t in targets if '__' not in t]
  standard_targets = [
      t for t in standard_targets if t.endswith(const.TEST_TARGET_SUFFIXES)
      or t in const.TEST_TARGET_ALLOWLIST
  ]
  all_test_targets.update(standard_targets)

  # Find targets using internal GN suffixes (e.g., Java APKs).
  _SUBTARGET_SUFFIXES = (
      '__java_binary',  # robolectric_binary()
      '__test_runner_script',  # test() targets
      '__test_apk',  # instrumentation_test_apk() targets
  )
  for suffix in _SUBTARGET_SUFFIXES:
    all_test_targets.update(t[:-len(suffix)] for t in targets
                            if t.endswith(suffix))

  return sorted(list(all_test_targets))


def _ParseRefsOutput(output: str) -> list[str]:
  targets: list[str] = output.splitlines()
  # Filter out any warnings messages. E.g. those about unused GN args.
  # https://crbug.com/444024516
  targets = [t for t in targets if t.startswith('//')]
  return targets


def FindTestTargets(target_cache: TargetCache, out_dir: str, paths: list[str],
                    args: argparse.Namespace) -> tuple[list[str], bool]:
  run_all: bool = args.run_all or args.run_changed
  target_index: int | None = args.target_index

  # Normalize paths, so they can be cached.
  paths = [os.path.realpath(p) for p in paths]
  test_targets: list[str] | None = target_cache.Find(paths)
  used_cache: bool = True
  if not test_targets:
    used_cache = False

    # Use gn refs to recursively find all targets that depend on |path|, filter
    # internal gn targets, and match against well-known test suffixes, falling
    # back to a list of known test targets if that fails.
    gn_path: str = os.path.join(str(const.DEPOT_TOOLS_DIR), 'gn.py')

    cmd: list[str] = [
        sys.executable,
        gn_path,
        'refs',
        out_dir,
        '--all',
        '--relation=source',
        '--relation=input',
    ] + paths
    targets: list[str] = _ParseRefsOutput(command.RunCommand(cmd))
    test_targets = _TestTargetsFromGnRefs(targets)

    # If no targets were identified as tests by looking at their names, ask GN
    # if any are executables.
    if not test_targets and targets:
      test_targets = _ParseRefsOutput(
          command.RunCommand(cmd + ['--type=executable']))

  if not test_targets:
    command.ExitWithMessage(
        f'"{paths}" did not match any test targets. Consider adding'
        f' one of the following targets to _TEST_TARGET_ALLOWLIST within '
        f'{__file__}: \n' + '\n'.join(targets))

  test_targets.sort()
  target_cache.Store(paths, test_targets)
  target_cache.Save()

  if len(test_targets) > 1:
    if run_all:
      print(f'Warning, found {len(test_targets)} test targets.',
            file=sys.stderr)
      if len(test_targets) > 10:
        command.ExitWithMessage('Your query likely involves non-test sources.')
      print('Trying to run all of them!', file=sys.stderr)
    elif target_index is not None and 0 <= target_index < len(test_targets):
      test_targets = [test_targets[target_index]]
    else:
      test_targets = [command.HaveUserPickTarget(paths, test_targets)]

  # Remove the // prefix to turn GN label into ninja target.
  test_targets_gn: list[str] = [t[2:] for t in test_targets]

  return (test_targets_gn, used_cache)


def RunTestTargets(out_dir: str, targets: list[str], gtest_filter: str,
                   pref_mapping_filter: str | None, extra_args: list[str],
                   dry_run: bool, no_try_android_wrappers: bool,
                   no_fast_local_dev: bool, no_single_variant: bool) -> None:

  for target in targets:
    target_binary: str = target.split(':')[1]

    # Look for the Android wrapper script first.
    path: str = os.path.join(out_dir, 'bin', f'run_{target_binary}')
    if no_try_android_wrappers or not os.path.isfile(path):
      # If the wrapper is not found or disabled use the Desktop target
      # which is an executable.
      path = os.path.join(out_dir, target_binary)
    else:
      if not no_fast_local_dev:
        # Usually want this flag when developing locally.
        extra_args = extra_args + ['--fast-local-dev']
      if not no_single_variant:
        extra_args = extra_args + ['--single-variant']

    cmd: list[str] = [path, f'--gtest_filter={gtest_filter}']
    if pref_mapping_filter:
      cmd.append(f'--test_policy_to_pref_mappings_filter={pref_mapping_filter}')
    cmd.extend(extra_args)

    print('Running test: ' + shlex.join(cmd))
    if not dry_run:
      command.StreamCommandOrExit(cmd)


def BuildCppTestFilter(filenames: list[str], line: int | None) -> str:
  make_filter_command: list[str | Path] = [
      sys.executable, const.SRC_DIR / 'tools' / 'make_gtest_filter.py'
  ]
  if line:
    make_filter_command += ['--line', str(line)]
  else:
    make_filter_command += ['--class-only']
  make_filter_command += filenames
  return command.RunCommand(make_filter_command).strip()


def BuildJavaTestFilter(filenames: list[str]) -> str:
  return ':'.join('*.{}*'.format(os.path.splitext(os.path.basename(f))[0])
                  for f in filenames)


_PREF_MAPPING_GTEST_FILTER: str = '*PolicyPrefsTest.PolicyToPrefsMapping*'

_PREF_MAPPING_FILE_REGEX: re.Pattern[str] = re.compile(
    const.PREF_MAPPING_FILE_PATTERN)

SPECIAL_TEST_FILTERS: list[tuple[re.Pattern[str], str]] = [
    (_PREF_MAPPING_FILE_REGEX, _PREF_MAPPING_GTEST_FILTER)
]


def BuildTestFilter(filenames: list[str], line: int | None) -> str:
  java_files: list[str] = [f for f in filenames if f.endswith('.java')]
  # TODO(crbug.com/434009870): Support EarlGrey tests, which don't use
  # Googletest's macros or pascal case naming convention.
  cc_files: list[str] = [
      f for f in filenames if f.endswith('.cc') or f.endswith('_unittest.mm')
  ]
  filters: list[str] = []
  if java_files:
    filters.append(BuildJavaTestFilter(java_files))
  if cc_files:
    filters.append(BuildCppTestFilter(cc_files, line))
  for regex, gtest_filter in SPECIAL_TEST_FILTERS:
    if any(True for f in filenames if regex.match(f)):
      filters.append(gtest_filter)
      break
  return ':'.join(filters)


def BuildPrefMappingTestFilter(filenames: list[str]) -> str | None:
  mapping_files: list[str] = [
      f for f in filenames if _PREF_MAPPING_FILE_REGEX.match(f)
  ]
  if not mapping_files:
    return None
  names_without_extension: list[str] = [Path(f).stem for f in mapping_files]
  return ':'.join(names_without_extension)


def GetChangedTestFiles() -> list[str]:
  # Find both committed and uncommitted changes.
  merge_base_command: list[str] = ['git', 'merge-base', 'origin/main', 'HEAD']
  merge_base: str = command.RunCommand(merge_base_command).strip()
  git_command: list[str] = [
      'git', 'diff', '--name-only', '--diff-filter=ACMRT', merge_base
  ]
  changed_files: list[str] = command.RunCommand(git_command).splitlines()

  test_files: list[str] = []
  for f in changed_files:
    if IsTestFile(f) is const.TestValidity.VALID_TEST:
      test_files.append(f)
  return test_files


@telemetry.tracer.start_as_current_span('chromium.tools.autotest.main')
def main():
  parser: argparse.ArgumentParser = argparse.ArgumentParser(
      prog='tools/autotest.py',
      description=__doc__,
      formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('--out-dir',
                      '--out_dir',
                      '--output-directory',
                      '--output_directory',
                      '-C',
                      metavar='OUT_DIR',
                      help='output directory of the build')
  parser.add_argument('--remote-search',
                      '--remote_search',
                      '-r',
                      action='store_true',
                      help='Search for tests using a remote service')
  parser.add_argument('--name',
                      action='append',
                      help='Search for the test by name, and apply test filter')
  parser.add_argument(
      '--run-all',
      '--run_all',
      action='store_true',
      help='Run all tests for the file or directory, instead of just one')
  parser.add_argument(
      '--target-index',
      '--target_index',
      type=int,
      help='When the target is ambiguous, choose the one with this index.')
  parser.add_argument(
      '--path-index',
      '--path_index',
      type=int,
      help='When the test path is ambiguous, choose the one with this index.')
  parser.add_argument(
      '--run-changed',
      '--run_changed',
      action='store_true',
      help='Run tests files modified since this branch diverged from main.')
  parser.add_argument('--line',
                      type=int,
                      help='run only the test on this line number. c++ only.')
  parser.add_argument('--gtest-filter',
                      '--gtest_filter',
                      '-f',
                      metavar='FILTER',
                      help='test filter')
  parser.add_argument('--test-policy-to-pref-mappings-filter',
                      '--test_policy_to_pref_mappings_filter',
                      metavar='FILTER',
                      help='policy pref mappings test filter')
  parser.add_argument(
      '--dry-run',
      '--dry_run',
      '-n',
      action='store_true',
      help='Print ninja and test run commands without executing them.')
  parser.add_argument(
      '--quiet',
      '-q',
      action='store_true',
      help='Do not print while building, only print if build fails.')
  parser.add_argument(
      '--no-try-android-wrappers',
      '--no_try_android_wrappers',
      action='store_true',
      help='Do not try to use Android test wrappers to run tests.')
  parser.add_argument('--no-fast-local-dev',
                      '--no_fast_local_dev',
                      action='store_true',
                      help='Do not add --fast-local-dev for Android tests.')
  parser.add_argument('--no-single-variant',
                      '--no_single_variant',
                      action='store_true',
                      help='Do not add --single-variant for Android tests.')
  parser.add_argument('files',
                      metavar='FILE_NAME',
                      nargs='*',
                      help='test suite file (eg. FooTest.java) or test name')

  args, _extras = parser.parse_known_args()

  if args.out_dir:
    constants.SetOutputDirectory(args.out_dir)
  constants.CheckOutputDirectory()
  out_dir: str = constants.GetOutDirectory()

  if not os.path.isdir(out_dir):
    parser.error(f'OUT_DIR "{out_dir}" does not exist.')
  target_cache: TargetCache = TargetCache(out_dir)

  if not args.run_changed and not args.files and not args.name:
    parser.error('Specify a file to test or use --run-changed')

  # Cog is almost unusable with local search, so turn on remote_search.
  use_remote_search: bool = args.remote_search
  if not use_remote_search and const.SRC_DIR.parts[:3] == ('/', 'google',
                                                           'cog'):
    if const.DEBUG:
      print('Detected cog, turning on remote-search.')
    use_remote_search = True

  gtest_filter: str | None = args.gtest_filter

  # Don't try to search if rg is not installed, and use the old behavior.
  if not use_remote_search and not shutil.which('rg'):
    if not args.quiet:
      print(
          'rg command not found. Install ripgrep to enable running tests by name.'
      )
    files_to_test = args.files
    test_names = []
  else:
    test_names = [f for f in args.files if not IsProbablyFile(f)]
    files_to_test = [f for f in args.files if IsProbablyFile(f)]

  if args.name:
    test_names.extend(args.name)
  if test_names:
    files, filter = SearchForTestsByName(test_names, args.quiet,
                                         use_remote_search)
    if not gtest_filter:
      gtest_filter = filter
    files_to_test.extend(files)

  if args.run_changed:
    files_to_test.extend(GetChangedTestFiles())
    # Remove duplicates.
    files_to_test = list(set(files_to_test))

  filenames: list[str] = []
  for file in files_to_test:
    filenames.extend(
        FindMatchingTestFiles(file, use_remote_search, args.path_index))

  if not filenames:
    command.ExitWithMessage('No associated test files found.')

  targets, used_cache = FindTestTargets(target_cache, out_dir, filenames, args)

  if not gtest_filter:
    gtest_filter = BuildTestFilter(filenames, args.line)

  if not gtest_filter:
    command.ExitWithMessage('Failed to derive a gtest filter')

  pref_mapping_filter: str | None = args.test_policy_to_pref_mappings_filter
  if not pref_mapping_filter:
    pref_mapping_filter = BuildPrefMappingTestFilter(filenames)

  assert targets

  build_ok: bool = BuildTestTargets(out_dir, targets, args.dry_run, args.quiet,
                                    False)

  # If we used the target cache, it's possible we chose the wrong target because
  # a gn file was changed. The build step above will check for gn modifications
  # and update build.ninja. Use this opportunity the verify the cache is still
  # valid.
  if used_cache and not target_cache.IsStillValid():
    target_cache = TargetCache(out_dir)
    new_targets, _ = FindTestTargets(target_cache, out_dir, filenames, args)
    if targets != new_targets:
      # Note that this can happen, for example, if you rename a test target.
      print('gn config was changed, trying to build again', file=sys.stderr)
      targets = new_targets
      build_ok: bool = BuildTestTargets(out_dir, targets, args.dry_run,
                                        args.quiet, True)

  telemetry.RecordMainAttributes(targets, gtest_filter, used_cache, out_dir)

  if not build_ok: sys.exit(1)

  RunTestTargets(out_dir, targets, gtest_filter, pref_mapping_filter, _extras,
                 args.dry_run, args.no_try_android_wrappers,
                 args.no_fast_local_dev, args.no_single_variant)


if __name__ == '__main__':
  telemetry.telemetry.initialize('chromium.tools.autotest')

  sys.exit(main())

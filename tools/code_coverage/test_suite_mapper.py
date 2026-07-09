#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""TestSuiteMapper: Maps a file to its target test suite."""

import argparse
import ast
from contextlib import contextmanager
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

SRC_ROOT = pathlib.Path(__file__).resolve().parents[2]
ISOLATE_MAP_PATH = (pathlib.Path('infra') / 'config' / 'generated' / 'testing' /
                    'gn_isolate_map.pyl')
UPDATE_RUST_PATH = pathlib.Path('tools') / 'rust' / 'update_rust.py'
RUST_TOOLCHAIN_PATH = pathlib.Path('third_party') / 'rust-toolchain'


def run_git(src_root, args):
  """Runs a git command and returns the CompletedProcess object.

  Args:
    src_root: Path to the git repository root.
    args: List of command line arguments for git.

  Returns:
    A subprocess.CompletedProcess object.
  """
  cmd = ['git'] + args
  return subprocess.run(
      cmd,
      capture_output=True,
      text=True,
      cwd=src_root,
      check=False,
      encoding='utf-8',
  )


def parse_isolate_map(content_str, source_name):
  """Parses isolate map content string.

  Args:
    content_str: The string content of the isolate map file (AST literal).
    source_name: Name or path of the source file (used for error logging).

  Returns:
    A dictionary mapping GN labels (strings) to test suite names (strings),
    or None if parsing fails.
  """
  try:
    data = ast.literal_eval(content_str)
    # Map label -> suite name, excluding additional_compile_target
    label_to_suite = {}
    if not isinstance(data, dict):
      print(f'Error parsing {source_name}: Expected a dictionary',
            file=sys.stderr)
      return None
    for suite, details in data.items():
      if isinstance(
          details, dict) and details.get('type') == 'additional_compile_target':
        continue
      if isinstance(details, dict):
        label = details.get('label')
        if label:
          label_to_suite[label] = suite
    return label_to_suite
  except Exception as e:
    print(f'Error parsing gn_isolate_map.pyl from {source_name}: {e}',
          file=sys.stderr)
    return None


def load_isolate_map(src_root):
  """Loads and parses the gn_isolate_map.pyl file from the workspace.

  Args:
    src_root: Path to the source root.

  Returns:
    A dictionary mapping GN labels to test suite names, or None if it fails.
  """
  src_root = pathlib.Path(src_root)
  pyl_path = src_root / ISOLATE_MAP_PATH
  if not pyl_path.exists():
    print(
        f'Error: Isolate map not found at {pyl_path}. '
        f'Please run main.star or sync.',
        file=sys.stderr,
    )
    return None

  try:
    with open(pyl_path, 'r', encoding='utf-8') as f:
      return parse_isolate_map(f.read(), str(pyl_path))
  except Exception as e:
    print(f'Error reading {pyl_path}: {e}', file=sys.stderr)
    return None


def run_gn_refs(src_root, build_dir, target_file):
  """Executes `gn refs` to retrieve the dependent targets closure.

  Args:
    src_root: Path to the source root.
    build_dir: Path to the build directory (e.g., out/Default).
    target_file: pathlib.Path to the target file to scan.

  Returns:
    A tuple (refs, error_message):
      refs: A list of strings (GN targets) if successful, or None.
      error_message: A string containing error output if failed, or None.
  """
  # Normalize target file to gn syntax
  gn_target = target_file.as_posix()
  if not gn_target.startswith('//'):
    gn_target = '//' + gn_target.lstrip('/')

  # Execute gn refs command
  cmd = ['gn', 'refs', build_dir, '--all', gn_target]
  try:
    res = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=src_root,
        check=False,
        encoding='utf-8',
    )
    if res.returncode != 0:
      return None, res.stderr.strip()
    return res.stdout.splitlines(), None
  except Exception as e:
    return None, str(e)


def common_dir_prefix_len(path1, path2):
  """Calculates the number of common leading directory components.

  Args:
    path1: First path string.
    path2: Second path string.

  Returns:
    Integer representing the number of matching path components from the
    root.
  """
  p1 = path1.strip('/').split('/')
  p2 = path2.strip('/').split('/')
  common = 0
  for c1, c2 in zip(p1, p2):
    if c1 == c2:
      common += 1
    else:
      break
  return common


def extract_test_suites(gn_refs_output, file_path, isolate_map):
  """Scans and filters transitive GN targets for executable test suites.

  Heuristic: Prefers test suites that are in a closer directory to the
  target file.

  Args:
    gn_refs_output: List of strings representing GN targets.
    file_path: pathlib.Path of the target file.
    isolate_map: Dictionary mapping GN labels to test suite names.

  Returns:
    A list of strings representing the names of the test suites that cover
    the file, sorted by proximity.
  """
  resolved_suites = {}

  # Normalize file_path to relative path without leading // for comparison
  file_path_str = file_path.as_posix()
  file_dir = str(pathlib.Path(file_path_str.lstrip('/')).parent)

  for target in gn_refs_output:
    target = target.strip()
    if not target:
      continue

    parts = target.split(':')
    target_dir = parts[0].lstrip('/') if len(parts) > 1 else ''

    # Look up in isolate map
    if target in isolate_map:
      suite_name = isolate_map[target]
      # Score based on how many directory components are shared.
      # The 'score' is the depth of the common directory prefix between the
      # file and the target.
      score = common_dir_prefix_len(file_dir, target_dir)
      resolved_suites[suite_name] = max(resolved_suites.get(suite_name, -1),
                                        score)

  if not resolved_suites:
    return []

  max_score = max(resolved_suites.values())

  # Filter: if we have any matches with score > 0, discard matches with
  # score == 0 (e.g., targets in the root directory).
  if max_score > 0:
    filtered_suites = [
        name for name, score in resolved_suites.items() if score > 0
    ]
  else:
    filtered_suites = list(resolved_suites.keys())

  return sorted(filtered_suites)


def get_build_gn_paths(gn_refs_output):
  """Extracts unique BUILD.gn file paths from GN targets.

  Args:
    gn_refs_output: List of strings representing GN targets.

  Returns:
    A sorted list of strings representing relative paths to BUILD.gn files.
  """
  build_gns = set()
  for target in gn_refs_output:
    target = target.strip()
    if not target.startswith('//'):
      continue
    path_part = target[2:]
    parts = path_part.split(':')
    dir_path = parts[0]
    if dir_path:
      build_gns.add(dir_path + '/BUILD.gn')
    else:
      build_gns.add('BUILD.gn')
  return sorted(list(build_gns))


def check_graph_stability(src_root, revision, target_file, gn_refs_output):
  """Checks if BUILD.gn files or the target file have changed since revision.

  Args:
    src_root: Path to the source root.
    revision: The git revision to compare against.
    target_file: pathlib.Path to the target file.
    gn_refs_output: List of GN targets that depend on the file.

  Returns:
    A tuple (stable, changed_files):
      stable: Boolean indicating if the graph is stable.
      changed_files: List of strings (paths to changed files) if unstable.
  """
  build_gns = get_build_gn_paths(gn_refs_output)
  rel_target_file = target_file.lstrip('/')
  paths_to_check = [rel_target_file] + build_gns
  cmd = ['diff', '--name-only', f'{revision}..HEAD', '--'] + paths_to_check
  res = run_git(src_root, cmd)
  if res.returncode != 0:
    print(
        f'Warning: Stability check failed to run: {res.stderr.strip()}',
        file=sys.stderr,
    )
    return False, []
  changed_files = res.stdout.splitlines()
  return len(changed_files) == 0, changed_files


def get_ignored_paths(src_root):
  """Finds ignored file and directory paths in the main checkout.

  Args:
    src_root: Path to the source root.

  Returns:
    A list of strings representing relative paths to ignored directories/files.
  """
  res = run_git(src_root, ['status', '--ignored', '--porcelain'])
  ignored_paths = []
  if res.returncode == 0:
    for line in res.stdout.splitlines():
      if line.startswith('!! '):
        path = line[3:]
        if path.endswith('/'):
          path = path[:-1]
        ignored_paths.append(path)
  return ignored_paths


def find_nested_repos_and_symlinks(src_root):
  """Finds nested git repos and symlinks in the checkout up to depth 5.

  Args:
    src_root: Path to the source root.

  Returns:
    A list of strings representing relative paths to nested repos/symlinks.
  """
  src_root = pathlib.Path(src_root)
  targets = []
  max_depth = 5
  for root, dirs, files in os.walk(src_root):
    root_path = pathlib.Path(root)
    rel_path = root_path.relative_to(src_root)
    parts = rel_path.parts

    # Skip ignored directories and out/
    if parts:
      if parts[0] in ('out', '.git'):
        dirs.clear()
        continue
      if len(parts) >= max_depth:
        dirs.clear()
        continue

    # Check for git repo
    if '.git' in files or '.git' in dirs:
      if parts:
        targets.append(rel_path.as_posix())
        dirs.clear()
        continue
      else:
        if '.git' in dirs:
          dirs.remove('.git')

    # Check for symlinks
    for d in list(dirs):
      p = root_path / d
      if p.is_symlink():
        targets.append(p.relative_to(src_root).as_posix())
        dirs.remove(d)
    for f in files:
      p = root_path / f
      if p.is_symlink():
        targets.append(p.relative_to(src_root).as_posix())

  return targets


def _stub_missing_target(wt_path, target_ref):
  """Stubs a missing GN target in the worktree.

  Args:
    wt_path: pathlib.Path to the git worktree root.
    target_ref: GN target reference string (e.g., //foo/bar:baz).

  Returns:
    Boolean indicating if stubbing was successful.
  """
  if not target_ref.startswith('//'):
    return False

  parts = target_ref[2:].split(':')
  if len(parts) != 2:
    return False

  dir_path, target_name = parts
  dest_dir = wt_path / dir_path
  build_gn_path = dest_dir / 'BUILD.gn'

  stub_content = (f'\n# Stub added by test_suite_mapper.py\n'
                  f'group("{target_name}") {{}}\n')

  try:
    dest_dir.mkdir(parents=True, exist_ok=True)
    with open(build_gn_path, 'a', encoding='utf-8', newline='') as f:
      f.write(stub_content)
    print(f'Stubbed {target_ref} in {build_gn_path}')
    return True
  except Exception as e:
    print(f'Failed to stub {target_ref}: {e}', file=sys.stderr)
    return False


def _spoof_rust_version(wt_path, src_rust_toolchain_dir,
                        dest_rust_toolchain_dir):
  """Spoofs the Rust VERSION file in the worktree to match update_rust.py.

  Args:
    wt_path: pathlib.Path to the worktree root.
    src_rust_toolchain_dir: pathlib.Path to the source rust toolchain dir.
    dest_rust_toolchain_dir: pathlib.Path to the destination rust toolchain
      dir in worktree.

  Returns:
    Boolean indicating if spoofing was successful.
  """
  update_rust_path = wt_path / UPDATE_RUST_PATH
  if not update_rust_path.exists():
    return False

  # Parse expected version from update_rust.py
  content = update_rust_path.read_text(encoding='utf-8')
  rev_match = re.search(r"RUST_REVISION\s*=\s*'([0-9a-f]+)'", content)
  sub_match = re.search(r'RUST_SUB_REVISION\s*=\s*([0-9]+)', content)
  if not (rev_match and sub_match):
    return False

  expected_rev = rev_match.group(1)
  expected_sub = sub_match.group(1)

  # Read live VERSION
  src_version_path = src_rust_toolchain_dir / 'VERSION'
  if not src_version_path.exists():
    return False

  version_content = src_version_path.read_text(encoding='utf-8').strip()

  # Example VERSION format:
  # rustc 1.79.0-dev 49b139rt3 (12487ec81-1-1256 clbr987s chromium)
  pattern = r'rustc (\S+) (\S+) \(([0-9a-f]+)-([0-9]+)-(\S+) chromium\)'
  match = re.match(pattern, version_content)
  if not match:
    return False

  rust_version = match.group(1)
  clang_part = match.group(5)
  new_version_content = (
      f'rustc {rust_version} {expected_rev} '
      f'({expected_rev}-{expected_sub}-{clang_part} chromium)\n')

  dest_version_path = dest_rust_toolchain_dir / 'VERSION'
  try:
    dest_rust_toolchain_dir.mkdir(parents=True, exist_ok=True)
    dest_version_path.write_text(new_version_content, encoding='utf-8')
    return True
  except Exception as e:
    print(f'Failed to write spoofed Rust VERSION: {e}', file=sys.stderr)
    return False


def _setup_worktree_symlinks(src_root, wt_path, build_dir):
  """Creates symlinks in the worktree for ignored paths and nested repos.

  Args:
    src_root: Path to the source root.
    wt_path: pathlib.Path to the worktree root.
    build_dir: Path to the build directory.
  """
  ignored_paths = get_ignored_paths(src_root)
  extra_targets = find_nested_repos_and_symlinks(src_root)
  all_paths = set(ignored_paths + extra_targets)

  exclude_prefixes = ['out']
  build_dir_parts = pathlib.Path(build_dir).parts
  if build_dir_parts:
    exclude_prefixes.append(build_dir_parts[0])

  sorted_paths = sorted(all_paths, key=lambda p: len(pathlib.Path(p).parts))

  for rel_path in sorted_paths:
    if rel_path in ('', '.'):
      continue

    parts = pathlib.Path(rel_path).parts
    if parts and parts[0] in exclude_prefixes:
      continue

    target = src_root / rel_path
    link = wt_path / rel_path

    if rel_path == RUST_TOOLCHAIN_PATH.as_posix():
      if (wt_path / UPDATE_RUST_PATH).exists():
        link.mkdir(parents=True, exist_ok=True)
        for child in target.iterdir():
          if child.name == 'VERSION':
            continue
          child_link = link / child.name
          if child_link.exists() or child_link.is_symlink():
            if child_link.is_dir() and not child_link.is_symlink():
              shutil.rmtree(child_link)
            else:
              child_link.unlink()
          child_link.symlink_to(child, target_is_directory=child.is_dir())

        if _spoof_rust_version(wt_path, target, link):
          continue
        else:
          shutil.rmtree(link)
          # Fallthrough to standard symlink creation

    # Skip if any parent directory is already a symlink
    parent_is_symlink = False
    for parent in link.parents:
      if parent == wt_path:
        break
      if parent.is_symlink():
        parent_is_symlink = True
        break
    if parent_is_symlink:
      continue

    link.parent.mkdir(parents=True, exist_ok=True)
    if link.exists() or link.is_symlink():
      if link.is_dir() and not link.is_symlink():
        shutil.rmtree(link)
      else:
        link.unlink()
    if target.exists():
      link.symlink_to(target, target_is_directory=target.is_dir())


def _run_gn_gen_with_stubbing(wt_path, build_dir):
  """Runs gn gen in the worktree, attempting to stub missing targets.

  Args:
    wt_path: pathlib.Path to the worktree root.
    build_dir: Path to the build directory.

  Returns:
    Boolean indicating if gn gen succeeded (possibly after stubbing).
  """
  stubbed_targets = set()
  max_retries = 10
  for attempt in range(max_retries):
    res = subprocess.run(
        ['gn', 'gen', build_dir],
        capture_output=True,
        text=True,
        cwd=wt_path,
        check=False,
        encoding='utf-8',
    )
    if res.returncode == 0:
      return True

    # Check for unresolved dependencies
    if 'ERROR Unresolved dependencies.' in res.stdout:
      missing_targets = []
      # Regex to find missing targets, e.g., "//foo:bar" or
      # "//foo:bar(//toolchain)"
      pattern = r'needs\s+([\w./:-]+)(?:\([\w./:-]+\))?'
      for line in res.stdout.splitlines():
        match = re.search(pattern, line)
        if match:
          missing_targets.append(match.group(1))

      if missing_targets:
        stubbed_any = False
        for t in set(missing_targets):
          if t in stubbed_targets:
            print(
                f'Error: Target {t} was already stubbed but '
                f'still unresolved. Cannot recover.',
                file=sys.stderr,
            )
            return False
          if _stub_missing_target(wt_path, t):
            stubbed_targets.add(t)
            stubbed_any = True
        if stubbed_any:
          print(f'Retrying gn gen after stubbing... (Attempt {attempt + 1})')
          continue

    print(
        f'Error running gn gen in worktree (Attempt {attempt + 1}): '
        f'{res.stdout.strip()}',
        file=sys.stderr,
    )
    if res.stderr:
      print(res.stderr.strip(), file=sys.stderr)
    break
  else:
    print(
        'Error: Failed to resolve GN graph after maximum retries.',
        file=sys.stderr,
    )
  return False


@contextmanager
def git_worktree(src_root, revision, prefix='wt_cov_map_', parent_dir=None):
  """Context manager for creating and cleaning up a git worktree.

  Args:
    src_root: Path to the source root.
    revision: Git revision to checkout in the worktree.
    prefix: Prefix for the temporary directory name.
    parent_dir: Parent directory for the temporary directory (defaults to
      system temp).

  Yields:
    pathlib.Path to the created worktree.
  """
  with tempfile.TemporaryDirectory(prefix=prefix, dir=parent_dir) as wt_dir:
    wt_path = pathlib.Path(wt_dir)

    res = run_git(
        src_root,
        ['worktree', 'add', '--detach', '--force',
         str(wt_path), revision],
    )
    if res.returncode != 0:
      raise RuntimeError(f'Error creating git worktree: {res.stderr.strip()}')

    try:
      yield wt_path
    finally:
      run_git(src_root, ['worktree', 'remove', '--force', str(wt_path)])


def run_heavyweight_resolution(src_root, revision, build_dir, target_file):
  """Runs GN graph resolution at a revision by checking out a worktree.

  Args:
    src_root: Path to the source root.
    revision: Git revision to perform the mapping at.
    build_dir: Path to the build directory.
    target_file: pathlib.Path to the target file.

  Returns:
    A list of strings (test suites) if successful, or None.
  """
  try:
    with git_worktree(src_root, revision,
                      parent_dir=src_root.parent) as wt_path:
      # 2. Create symlinks for non-tracked/nested repo paths
      _setup_worktree_symlinks(src_root, wt_path, build_dir)

      # 3. Copy args.gn
      wt_build_dir = wt_path / build_dir
      wt_build_dir.mkdir(parents=True, exist_ok=True)
      shutil.copy2(src_root / build_dir / 'args.gn', wt_build_dir / 'args.gn')

      # 4. Run gn gen (with auto-recovery for missing targets)
      if not _run_gn_gen_with_stubbing(wt_path, build_dir):
        return None

      # 5. Run gn refs
      refs, err = run_gn_refs(wt_path, build_dir, target_file)
      if err:
        print(f'Error running gn refs in worktree: {err}', file=sys.stderr)
        return None
      if refs is None:
        return None

      # 6. Load isolate map at revision (from worktree)
      pyl_path = wt_path / ISOLATE_MAP_PATH
      if not pyl_path.exists():
        print(f'Error: Isolate map not found in worktree.', file=sys.stderr)
        return None

      try:
        with open(pyl_path, 'r', encoding='utf-8') as f:
          isolate_map = parse_isolate_map(f.read(), str(pyl_path))
      except Exception as e:
        print(f'Error reading isolate map from worktree: {e}', file=sys.stderr)
        return None

      if isolate_map is None:
        return None

      # 7. Extract test suites
      return extract_test_suites(refs, target_file, isolate_map)
  except Exception as e:
    print(e, file=sys.stderr)
    return None


def verify_build_dir(src_root, build_dir):
  """Verifies that build_dir is configured to compile for Linux.

  Args:
    src_root: Path to the source root.
    build_dir: Path to the build directory.

  Returns:
    A tuple (success, error_message):
      success: Boolean indicating if configuration is valid.
      error_message: String containing error detail if invalid, or None.
  """
  src_root = pathlib.Path(src_root)
  args_gn_path = src_root / build_dir / 'args.gn'
  if not args_gn_path.exists():
    return (
        False,
        f'args.gn not found in {build_dir}. Please run `gn gen`.',
    )

  target_os = None
  try:
    with open(args_gn_path, 'r', encoding='utf-8') as f:
      for line in f:
        line = line.split('#')[0].strip()
        if not line:
          continue
        parts = line.split('=', 1)
        if len(parts) == 2:
          key = parts[0].strip()
          val = parts[1].strip().strip('"\'')
          if key == 'target_os':
            target_os = val
            break
  except Exception as e:
    return False, f'Error reading {args_gn_path}: {e}'

  if target_os and target_os != 'linux':
    return (
        False,
        f'Unsupported target_os: {target_os}. '
        f"Only 'linux' is supported initially.",
    )

  if not target_os and not sys.platform.startswith('linux'):
    return (
        False,
        f'Host platform {sys.platform} is not Linux, '
        f"and target_os is not set to 'linux' in args.gn.",
    )

  return True, None


def _load_isolate_map_for_args(src_root, isolate_map_file, revision,
                               force_heavyweight):
  """Loads the isolate map based on arguments.

  Args:
    src_root: Path to the source root.
    isolate_map_file: Path to a local isolate map file, or None.
    revision: Git revision, or None.
    force_heavyweight: Boolean indicating if heavyweight resolution is
      forced.

  Returns:
    A dictionary mapping GN labels to test suite names, or None if it should
    be skipped (for heavyweight) or failed.
  """
  if isolate_map_file:
    if not os.path.exists(isolate_map_file):
      print(
          f'Error: Isolate map file not found: {isolate_map_file}',
          file=sys.stderr,
      )
      sys.exit(1)
    try:
      with open(isolate_map_file, 'r', encoding='utf-8') as f:
        return parse_isolate_map(f.read(), os.fspath(isolate_map_file))
    except Exception as e:
      print(f'Error reading isolate map file: {e}', file=sys.stderr)
      sys.exit(1)
  elif revision:
    if not force_heavyweight:
      print(
          'Error: For lightweight mapping at a revision, you must provide '
          'the historical isolate map using --isolate-map-file.',
          file=sys.stderr,
      )
      sys.exit(1)
    return None
  else:
    return load_isolate_map(src_root)


def _check_and_warn_stability(src_root, revision, file_path, refs):
  """Checks graph stability and prints warnings if unstable.

  Args:
    src_root: Path to the source root.
    revision: Git revision.
    file_path: pathlib.Path of the target file.
    refs: List of GN targets that depend on the file.

  Returns:
    Boolean indicating if the graph is stable.
  """
  stable, changed = check_graph_stability(src_root, revision, file_path, refs)
  if not stable:
    print(
        f'Warning: The GN graph or the file may have changed '
        f'since revision {revision}.',
        file=sys.stderr,
    )
    changed_files_display = ', '.join(changed[:5])
    if len(changed) > 5:
      changed_files_display += f" and {len(changed) - 5} more"
    print(
        f'The following relevant files were modified: '
        f'{changed_files_display}',
        file=sys.stderr,
    )
    print(
        'The mapping results might be inaccurate.',
        file=sys.stderr,
    )
  return stable


def _should_run_heavyweight(src_root, revision, file_path, refs,
                            force_heavyweight, stable, lightweight_suites):
  """Determines if heavyweight resolution should be run.

  Args:
    src_root: Path to the source root.
    revision: Git revision, or None.
    file_path: pathlib.Path of the target file.
    refs: List of GN targets that depend on the file.
    force_heavyweight: Boolean indicating if heavyweight resolution is
      forced.
    stable: Boolean indicating if the graph is stable.
    lightweight_suites: List of test suites from lightweight mapping.

  Returns:
    Boolean indicating if heavyweight resolution should be run.
  """
  if force_heavyweight:
    return True

  if revision and not stable:
    head_isolate_map = load_isolate_map(src_root)
    if head_isolate_map and lightweight_suites is not None:
      current_suites = extract_test_suites(refs, file_path, head_isolate_map)
      if lightweight_suites != current_suites:
        print(
            'Difference detected between lightweight revision mapping and '
            'HEAD mapping, and the build graph is unstable. Triggering '
            'heavyweight resolution.',
            file=sys.stderr,
        )
        return True
  return False


def _execute_heavyweight_resolution(src_root, revision, build_dir, file_path,
                                    lightweight_suites):
  """Executes heavyweight resolution and handles fallback/logging.

  Args:
    src_root: Path to the source root.
    revision: Git revision.
    build_dir: Path to the build directory.
    file_path: pathlib.Path of the target file.
    lightweight_suites: List of test suites from lightweight mapping.

  Returns:
    A list of strings (test suites).
  """
  if not revision:
    print('Error: Heavyweight resolution requires a --revision.',
          file=sys.stderr)
    sys.exit(1)
  print(
      f'Running heavyweight GN graph resolution at revision {revision}...',
      file=sys.stderr,
  )
  print(
      'This requires checking out a worktree and running gn gen '
      '(takes ~1 minute).',
      file=sys.stderr,
  )
  heavyweight_suites = run_heavyweight_resolution(src_root, revision, build_dir,
                                                  file_path)
  if heavyweight_suites is not None:
    if (lightweight_suites is not None
        and heavyweight_suites != lightweight_suites):
      print(
          f'Heavyweight resolution completed. Results corrected: '
          f'{lightweight_suites} -> {heavyweight_suites}',
          file=sys.stderr,
      )
    else:
      print(
          f'Heavyweight resolution completed. '
          f'Confirmed results: {heavyweight_suites}',
          file=sys.stderr,
      )
    return heavyweight_suites
  else:
    print(
        'Error: Heavyweight resolution failed. '
        'Falling back to lightweight results.',
        file=sys.stderr,
    )
    return lightweight_suites if lightweight_suites is not None else []


def main():
  parser = argparse.ArgumentParser(
      description='Maps a Chromium file path to its covering test suites.')
  parser.add_argument(
      'file_path',
      type=pathlib.Path,
      help='Relative path to the target file (e.g., chrome/browser/.../file.cc)'
  )
  parser.add_argument(
      '--build-dir',
      type=pathlib.Path,
      default=pathlib.Path('out/Default'),
      help=('Build directory (defaults to out/Default). Must contain args.gn '
            'and be configured for target_os="linux".'),
  )
  parser.add_argument(
      '--revision',
      '-r',
      help='Git revision (hash, tag, branch) to perform the mapping at.',
  )
  parser.add_argument(
      '--force-heavyweight',
      action='store_true',
      help=('Force rebuilding the GN build graph at the revision '
            '(slower but 100% accurate).'),
  )
  parser.add_argument(
      '--isolate-map-file',
      type=pathlib.Path,
      help='Path to a local gn_isolate_map.pyl file to use for mapping.',
  )
  args = parser.parse_args()

  src_root = SRC_ROOT
  build_dir = args.build_dir
  revision = args.revision

  if revision:
    res = run_git(src_root, ['rev-parse', '--verify', revision])
    if res.returncode != 0:
      print(f'Error: Invalid revision: {revision}', file=sys.stderr)
      sys.exit(1)
    revision = res.stdout.strip()

  # Verify build directory exists
  if not (src_root / build_dir).exists():
    print(
        f'Error: Build directory {build_dir} does not exist or is not '
        f'configured.',
        file=sys.stderr,
    )
    print(
        f'Please run `gn gen {build_dir}` or supply a valid --build-dir.',
        file=sys.stderr,
    )
    sys.exit(1)

  success, err = verify_build_dir(src_root, build_dir)
  if not success:
    print(f'Error: {err}', file=sys.stderr)
    sys.exit(1)

  isolate_map = _load_isolate_map_for_args(src_root, args.isolate_map_file,
                                           revision, args.force_heavyweight)

  if isolate_map is None and not args.force_heavyweight:
    sys.exit(1)

  # Run references scan (uses current workspace GN graph)
  refs, err = run_gn_refs(src_root, build_dir, args.file_path)
  if err:
    print(f'Error running GN refs query: {err}', file=sys.stderr)
    sys.exit(1)

  if not refs:
    print(json.dumps([]))
    sys.exit(0)

  stable = True
  if revision:
    stable = _check_and_warn_stability(src_root, revision, args.file_path, refs)

  lightweight_suites = None
  if isolate_map:
    lightweight_suites = extract_test_suites(refs, args.file_path, isolate_map)

  run_heavyweight = _should_run_heavyweight(
      src_root,
      revision,
      args.file_path,
      refs,
      args.force_heavyweight,
      stable,
      lightweight_suites,
  )

  if run_heavyweight:
    test_suites = _execute_heavyweight_resolution(src_root, revision, build_dir,
                                                  args.file_path,
                                                  lightweight_suites)
  else:
    test_suites = lightweight_suites if lightweight_suites is not None else []

  print(json.dumps(test_suites, indent=2))


if __name__ == '__main__':
  main()

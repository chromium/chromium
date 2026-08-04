#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compiles profiles and generates coverage reports for local test binaries."""

import argparse
import os
import pathlib
import subprocess
import sys


def find_llvm_tool(tool_name: str) -> pathlib.Path:
  """Locates pinned LLVM tool inside third_party.

    Args:
        tool_name: Executable name (e.g., 'llvm-profdata').

    Returns:
        Absolute path to the pinned tool executable.
    """
  base = pathlib.Path(__file__).resolve().parents[4]
  tool_path = base / 'third_party' / 'llvm-build' / 'Release+Asserts' / 'bin'
  exec_path = tool_path / tool_name
  if not exec_path.exists():
    print(
        f'Downloading missing LLVM coverage tool ({tool_name})...',
        file=sys.stderr,
    )
    update_script = base / 'tools' / 'clang' / 'scripts' / 'update.py'
    try:
      subprocess.check_call([
          sys.executable,
          str(update_script),
          '--package',
          'coverage_tools',
      ])
    except subprocess.CalledProcessError as e:
      print(f'Failed to download LLVM coverage tools: {e}', file=sys.stderr)
      sys.exit(1)

    if not exec_path.exists():
      print(f'LLVM tool still not found at {exec_path}', file=sys.stderr)
      sys.exit(1)
  return exec_path


def run_test_binary(
    binary_path: pathlib.Path,
    profraw_dir: pathlib.Path,
    extra_args: list[str],
) -> int:
  """Runs local test binary with LLVM_PROFILE_FILE environment variable.

    Args:
        binary_path: Path to executable test binary.
        profraw_dir: Scratch directory to store raw profiles.
        extra_args: Additional arguments passed to the binary.

    Returns:
        Subprocess return code integer.
    """
  env = os.environ.copy()
  prof_template = profraw_dir / f'{binary_path.name}.%4m%c.profraw'
  env['LLVM_PROFILE_FILE'] = str(prof_template)
  cmd = [str(binary_path)] + extra_args
  res = subprocess.run(cmd, env=env, check=False)
  return res.returncode


def merge_profiles(
    profraw_dir: pathlib.Path,
    output_profdata: pathlib.Path,
) -> bool:
  """Merges profraw files into indexed profdata artifact.

    Args:
        profraw_dir: Directory containing .profraw files.
        output_profdata: Output .profdata file path.

    Returns:
        True if successful, False otherwise.
    """
  profdata = find_llvm_tool('llvm-profdata')
  raw_files = list(profraw_dir.glob('*.profraw'))
  if not raw_files:
    print(f'No profraw files found in {profraw_dir}', file=sys.stderr)
    return False
  cmd = [str(profdata), 'merge', '-o', str(output_profdata), '--sparse']
  cmd += [str(f) for f in raw_files]
  res = subprocess.run(cmd, check=False)
  return res.returncode == 0


def export_coverage(
    binary_path: pathlib.Path,
    profdata_path: pathlib.Path,
    source_file: str | None,
) -> str | None:
  """Runs llvm-cov export to return text report or summary.

    Args:
        binary_path: Path to instrumented binary.
        profdata_path: Path to merged .profdata artifact.
        source_file: Optional source file filter.

    Returns:
        Exported coverage text string or None on error.
    """
  cov_tool = find_llvm_tool('llvm-cov')
  cmd = [
      str(cov_tool), 'show',
      str(binary_path), f'-instr-profile={profdata_path}'
  ]
  if source_file:
    cmd.append(source_file)
  res = subprocess.run(cmd, capture_output=True, text=True, check=False)
  if res.returncode != 0:
    print(f'llvm-cov failed: {res.stderr}', file=sys.stderr)
    return None
  return res.stdout


def compile_binary(binary_path: pathlib.Path) -> bool:
  """Compiles target test binary locally using autoninja.

  Args:
      binary_path: Path to executable test binary.

  Returns:
      True if compilation succeeded, False otherwise.
  """
  build_dir = binary_path.parent
  target_name = binary_path.name
  print(f'Compiling target {target_name} in {build_dir}...', file=sys.stderr)
  cmd = ['autoninja', '-C', str(build_dir), target_name]
  res = subprocess.run(cmd, check=False)
  return res.returncode == 0


def main() -> None:
  """Parses CLI args and executes rapid local coverage generation."""
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--binary',
                      required=True,
                      type=pathlib.Path,
                      help='Path to test binary')
  parser.add_argument('--scratch',
                      type=pathlib.Path,
                      default=pathlib.Path('scratch/local_prof'),
                      help='Scratch directory for profiles')
  parser.add_argument('--source', help='Optional source file filter')
  parser.add_argument(
      '--no-compile',
      action='store_true',
      help='Skip autoninja target compilation step if binary is pre-built',
  )
  parser.add_argument('test_args',
                      nargs=argparse.REMAINDER,
                      help='Arguments passed to test binary')
  args = parser.parse_args()

  if not args.no_compile:
    if not compile_binary(args.binary):
      print(f'Failed to compile target {args.binary.name}', file=sys.stderr)
      sys.exit(1)

  args.scratch.mkdir(parents=True, exist_ok=True)
  code = run_test_binary(args.binary, args.scratch, args.test_args)
  if code != 0:
    print(f'Warning: test binary exited with {code}', file=sys.stderr)

  profdata = args.scratch / f'{args.binary.name}.profdata'
  if not merge_profiles(args.scratch, profdata):
    sys.exit(1)

  report = export_coverage(args.binary, profdata, args.source)
  if report:
    print(report)


if __name__ == '__main__':
  main()

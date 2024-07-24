#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test harness for chromium clang tools."""

from __future__ import print_function

import argparse
import difflib
import glob
import json
import os
import os.path
import re
import shutil
import subprocess
import sys


def _RunGit(args):
  if sys.platform == 'win32':
    args = ['git.bat'] + args
  else:
    args = ['git'] + args
  subprocess.check_call(args)


def _GenerateCompileCommands(files, include_paths):
  """Returns a JSON string containing a compilation database for the input."""
  # Note: in theory, backslashes in the compile DB should work but the tools
  # that write compile DBs and the tools that read them don't agree on the
  # escaping convention: https://llvm.org/bugs/show_bug.cgi?id=19687
  files = [f.replace('\\', '/') for f in files]
  include_path_flags = ' '.join('-I %s' % include_path.replace('\\', '/')
                                for include_path in include_paths)
  return json.dumps([{
      'directory':
      os.path.dirname(f),
      'command':
      'clang++ -std=c++20 -fsyntax-only %s %s' %
      (include_path_flags, os.path.basename(f)),
      'file':
      os.path.basename(f)
  } for f in files],
                    indent=2)


def _NumberOfTestsToString(tests):
  """Returns an English describing the number of tests."""
  return '%d test%s' % (tests, 's' if tests != 1 else '')


def _ApplyTool(tools_clang_scripts_directory, tool_to_test, tool_path,
               tool_args, test_directory_for_tool, actual_files, apply_edits,
               extract_edits_path):
  try:
    # Stage the test files in the git index. If they aren't staged, then
    # run_tool.py will skip them when applying replacements.
    args = ['add']
    args.extend(actual_files)
    _RunGit(args)

    # Launch the following pipeline if |apply_edits| is True:
    #     run_tool.py ... | extract_edits.py | apply_edits.py ...
    # Otherwise just the first step is done and the result is written to
    #   actual_files[0].
    processes = []
    args = ['python',
            os.path.join(tools_clang_scripts_directory, 'run_tool.py')]
    extra_run_tool_args_path = os.path.join(test_directory_for_tool,
                                            'run_tool.args')
    if os.path.exists(extra_run_tool_args_path):
      with open(extra_run_tool_args_path, 'r') as extra_run_tool_args_file:
        extra_run_tool_args = extra_run_tool_args_file.readlines()
        args.extend([arg.strip() for arg in extra_run_tool_args])
    args.extend(['--tool', tool_to_test, '-p', test_directory_for_tool])

    if tool_path:
      args.extend(['--tool-path', tool_path])
    if tool_args:
      for arg in tool_args:
        args.append('--tool-arg=%s' % arg)

    args.extend(actual_files)
    processes.append(subprocess.Popen(args, stdout=subprocess.PIPE))

    if apply_edits:
      if not extract_edits_path:
        args = [
            'python',
            os.path.join(tools_clang_scripts_directory, 'extract_edits.py')
        ]
        processes.append(
            subprocess.Popen(args,
                             stdin=processes[-1].stdout,
                             stdout=subprocess.PIPE))
      else:
        args = ['python', os.path.join(extract_edits_path, 'extract_edits.py')]
        processes.append(
            subprocess.Popen(args,
                             stdin=processes[-1].stdout,
                             stdout=subprocess.PIPE))

      args = [
          'python',
          os.path.join(tools_clang_scripts_directory, 'apply_edits.py'), '-p',
          test_directory_for_tool
      ]
      args.extend(actual_files)  # Limit edits to the test files.
      processes.append(subprocess.Popen(
          args, stdin=processes[-1].stdout, stdout=subprocess.PIPE))

    # Wait for the pipeline to finish running + check exit codes.
    stdout, _ = processes[-1].communicate()
    for process in processes:
      process.wait()
      if process.returncode != 0:
        print('Failure while running the tool.')
        return process.returncode

    if apply_edits:
      # Reformat the resulting edits via: git cl format.
      args = ['cl', 'format']
      args.extend(actual_files)
      _RunGit(args)
    else:
      with open(actual_files[0], 'w') as output_file:
        output_file.write(stdout.decode('utf-8'))

    return 0

  finally:
    # No matter what, unstage the git changes we made earlier to avoid polluting
    # the index.
    args = ['reset', '--quiet', 'HEAD']
    args.extend(actual_files)
    _RunGit(args)


def _NormalizePathInRawOutput(path, test_dir):
  if not os.path.isabs(path):
    path = os.path.join(test_dir, path)

  return os.path.relpath(path, test_dir)


def _NormalizeSingleRawOutputLine(output_line, test_dir):
  if not re.match('^[^:]+(:::.*){4,4}$', output_line):
    return output_line

  edit_type, path, offset, length, replacement = output_line.split(':::', 4)
  path = _NormalizePathInRawOutput(path, test_dir)
  return "%s:::%s:::%s:::%s:::%s" % (edit_type, path, offset, length,
                                     replacement)


def _NormalizeRawOutput(output_lines, test_dir):
  return list(
      map(lambda line: _NormalizeSingleRawOutputLine(line, test_dir),
          output_lines))


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--apply-edits',
      action='store_true',
      help='Applies the edits to the original test files and compares the '
           'reformatted new files with the expected files.')
  parser.add_argument(
      '--tool-arg', nargs='?', action='append',
      help='optional arguments passed to the tool')
  parser.add_argument(
      '--tool-path', nargs='?',
      help='optional path to the tool directory')
  parser.add_argument('tool_name',
                      nargs=1,
                      help='Clang tool to be tested.')
  parser.add_argument(
      '--test-filter', default='*', help='optional glob filter for test names')
  parser.add_argument('--extract-edits-path',
                      nargs='?',
                      help='optional path to the extract_edits script\
      [e.g. if custom filtering or post-processing of edits is needed]')
  args = parser.parse_args(argv)
  tool_to_test = args.tool_name[0]
  print('\nTesting %s\n' % tool_to_test)
  tools_clang_scripts_directory = os.path.dirname(os.path.realpath(__file__))
  tools_clang_directory = os.path.dirname(tools_clang_scripts_directory)
  test_directory_for_tool = os.path.join(
      tools_clang_directory, tool_to_test, 'tests')
  compile_database = os.path.join(test_directory_for_tool,
                                  'compile_commands.json')
  source_files = glob.glob(
      os.path.join(test_directory_for_tool,
                   '%s-original.cc' % args.test_filter))
  ext = 'cc' if args.apply_edits else 'txt'
  actual_files = ['-'.join([source_file.rsplit('-', 1)[0], 'actual.cc'])
                  for source_file in source_files]
  expected_files = ['-'.join([source_file.rsplit('-', 1)[0], 'expected.' + ext])
                    for source_file in source_files]
  if not args.apply_edits and len(actual_files) != 1:
    print('Only one test file is expected for testing without apply-edits.')
    return 1

  include_paths = []
  include_paths.append(
      os.path.realpath(os.path.join(tools_clang_directory, '../..')))
  # Many gtest and gmock headers expect to have testing/gtest/include and/or
  # testing/gmock/include in the include search path.
  include_paths.append(
      os.path.realpath(os.path.join(tools_clang_directory,
                                    '../..',
                                    'testing/gtest/include')))
  include_paths.append(
      os.path.realpath(os.path.join(tools_clang_directory,
                                    '../..',
                                    'testing/gmock/include')))

  include_paths.append(
      os.path.realpath(
          os.path.join(tools_clang_directory, '../..',
                       'third_party/googletest/src/googletest/include')))

  include_paths.append(
      os.path.realpath(
          os.path.join(tools_clang_directory, '../..',
                       'third_party/googletest/src/googlemock/include')))

  if len(actual_files) == 0:
    print('Tool "%s" does not have compatible test files.' % tool_to_test)
    return 1

  # Set up the test environment.
  for source, actual in zip(source_files, actual_files):
    shutil.copyfile(source, actual)
  # Generate a temporary compilation database to run the tool over.
  with open(compile_database, 'w') as f:
    f.write(_GenerateCompileCommands(actual_files, include_paths))

  # Run the tool.
  os.chdir(test_directory_for_tool)
  exitcode = _ApplyTool(tools_clang_scripts_directory, tool_to_test,
                        args.tool_path, args.tool_arg, test_directory_for_tool,
                        actual_files, args.apply_edits, args.extract_edits_path)
  if (exitcode != 0):
    return exitcode

  # Compare actual-vs-expected results.
  passed = 0
  failed = 0
  for expected, actual in zip(expected_files, actual_files):
    print('[ RUN      ] %s' % os.path.relpath(actual))
    expected_output = actual_output = None
    with open(expected, 'r') as f:
      expected_output = f.readlines()
    with open(actual, 'r') as f:
      actual_output =  f.readlines()
    if not args.apply_edits:
      actual_output = _NormalizeRawOutput(actual_output,
                                          test_directory_for_tool)
      expected_output = _NormalizeRawOutput(expected_output,
                                            test_directory_for_tool)
    if actual_output != expected_output:
      failed += 1
      lines = difflib.unified_diff(expected_output, actual_output,
                                   fromfile=os.path.relpath(expected),
                                   tofile=os.path.relpath(actual))
      sys.stdout.writelines(lines)
      print('[  FAILED  ] %s' % os.path.relpath(actual))
      # Don't clean up the file on failure, so the results can be referenced
      # more easily.
      continue
    print('[       OK ] %s' % os.path.relpath(actual))
    passed += 1
    os.remove(actual)

  if failed == 0:
    os.remove(compile_database)

  print('[==========] %s ran.' % _NumberOfTestsToString(len(source_files)))
  if passed > 0:
    print('[  PASSED  ] %s.' % _NumberOfTestsToString(passed))
  if failed > 0:
    print('[  FAILED  ] %s.' % _NumberOfTestsToString(failed))
    return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import shutil
import subprocess
import sys

_RSP_RE = re.compile(r' (@(.+?\.rsp)) ')
_CLANG_WRAPPER_CMD_LINE_RE = re.compile(
    r'''
    (
      (?P<rewrapper>.*rewrapper(\.exe)?"?\s+)
      # rewrapper may have args between it and clang.
      # Assume the args do not contain spaces.
      (?P<rewrapper_arg>\-\S+\s+)*
    )?
    # Assume the path to clang does not contain spaces.
    (?P<clang>\S*clang\S*)
    \s+
    (?P<args>.*)
    ''', re.VERBOSE)
_debugging = False


def _IsTargettingWindows(target_os):
  if target_os is not None:
    # Available choices are based on: gn help target_os
    assert target_os in [
        'android',
        'chromeos',
        'fuchsia',
        'ios',
        'linux',
        'mac',
        'nacl',
        'win',
    ]
    return target_os == 'win'
  return sys.platform == 'win32'


def _FilterFlags(command, additional_filtered_flags):
  # Dictionary from flags to filter, to the number of additional arguments each
  # flag consumes (so we can remove any flag parameters).
  flags_to_filter = {
      # These are Visual Studio-specific arguments not recognized or used by
      # some third-party clang tooling. They only suppress or activate graphical
      # output anyway.
      '/nologo': 0,
      '/showIncludes': 0,
      # Drop frontend-only arguments, which generally aren't needed by clang
      # tooling.
      '-Xclang': 1,
      # This is used for profiling-guided optimizations. Not necessary by tools,
      # and clangd complains it cannot find the referenced profile file.
      '-fprofile-sample-use': 1,
      # This flag is only usable with -fprofile-sample-use excluded above.
      # Exclude it to avoid having an unused-command-line-argument error.
      '-fsample-profile-use-profi': 1,
  }
  # Add user-added flags. We only support flags with no parameters here.
  if additional_filtered_flags:
    flags_to_filter.update((flag, 0) for flag in additional_filtered_flags)

  filtered_command_parts = []
  parts_to_consume = 0
  for command_part in command.split():
    # Consume flag parameters.
    if parts_to_consume > 0:
      parts_to_consume -= 1
      continue
    # Handle -flag=parameter syntax. We only support a single parameter here.
    split_flag = command_part.split("=", 1)
    if len(split_flag) == 2 and split_flag[0] in flags_to_filter:
      expected_params = flags_to_filter[split_flag[0]]
      if expected_params == 1:
        continue
      elif _debugging:
        print("Expecting %s to have %d parameters, but got 1" %
              (split_flag[0], expected_params))
        print("The flag will be kept in the command!")
    # Handle regular parameters.
    if command_part in flags_to_filter:
      parts_to_consume = flags_to_filter[command_part]
      continue
    # This command part is not in the filter list, nor should be consumed as a
    # flag parameter.
    filtered_command_parts.append(command_part)

  return ' '.join(filtered_command_parts)


def _ProcessCommand(command, filtered_args, target_os):
  # If the driver mode is not already set then define it. Driver mode is
  # automatically included in the compile db by clang starting with release
  # 9.0.0.
  driver_mode = ''
  # Only specify for Windows. Other platforms do fine without it.
  if _IsTargettingWindows(target_os) and '--driver-mode' not in command:
    driver_mode = '--driver-mode=cl'

  # Removes rewrapper(.exe). On Windows inserts --driver-mode=cl as the
  # first arg.
  #
  # Deliberately avoid shlex.split() here, because it doesn't work predictably
  # for Windows commands (specifically, it doesn't parse args the same way that
  # Clang does on Windows).
  #
  # Instead, use a regex, with the simplifying assumption that the path to
  # clang-cl.exe contains no spaces.
  match = _CLANG_WRAPPER_CMD_LINE_RE.fullmatch(command)
  if match:
    match_dict = match.groupdict()
    command = ' '.join([match_dict['clang'], driver_mode, match_dict['args']])
  elif _debugging:
    print('Compile command didn\'t match expected regex!')
    print('Command:', command)
    print('Regex:', _CLANG_WRAPPER_CMD_LINE_RE.pattern)

  return _FilterFlags(command, filtered_args)


def _ProcessEntry(entry, filtered_args, target_os):
  """Transforms one entry in a compile db to be more clang-tool friendly.

  Expands the contents of the response file, if any, and performs any
  transformations needed to make the compile DB easier to use for third-party
  tooling.
  """
  # Expand the contents of the response file, if any.
  # http://llvm.org/bugs/show_bug.cgi?id=21634
  try:
    match = _RSP_RE.search(entry['command'])
    if match:
      rsp_path = os.path.join(entry['directory'], match.group(2))
      rsp_contents = open(rsp_path).read()
      entry['command'] = ''.join([
          entry['command'][:match.start(1)], rsp_contents,
          entry['command'][match.end(1):]
      ])
  except IOError:
    if _debugging:
      print('Couldn\'t read response file for %s' % entry['file'])

  entry['command'] = _ProcessCommand(entry['command'], filtered_args, target_os)

  return entry


def ProcessCompileDatabase(compile_db, filtered_args, target_os=None):
  """Make the compile db generated by ninja more clang-tool friendly.

  Args:
    compile_db: The compile database parsed as a Python dictionary.

  Returns:
    A postprocessed compile db that clang tooling can use.
  """
  compile_db = [_ProcessEntry(e, filtered_args, target_os) for e in compile_db]

  if not _IsTargettingWindows(target_os):
    return compile_db

  if _debugging:
    print('Read in %d entries from the compile db' % len(compile_db))
  original_length = len(compile_db)

  # Filter out NaCl stuff. The clang tooling chokes on them.
  # TODO(dcheng): This doesn't appear to do anything anymore, remove?
  compile_db = [
      e for e in compile_db if '_nacl.cc.pdb' not in e['command']
      and '_nacl_win64.cc.pdb' not in e['command']
  ]
  if _debugging:
    print('Filtered out %d entries...' % (original_length - len(compile_db)))

  # TODO(dcheng): Also filter out multiple commands for the same file. Not sure
  # how that happens, but apparently it's an issue on Windows.
  return compile_db


def GetNinjaPath():
  ninja_executable = 'ninja.exe' if sys.platform == 'win32' else 'ninja'
  return os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..',
                      '..', '..', 'third_party', 'ninja', ninja_executable)


# FIXME: This really should be a build target, rather than generated at runtime.
def GenerateWithNinja(path, targets=None):
  """Generates a compile database using ninja.

  Args:
    path: The build directory to generate a compile database for.
    targets: Additional targets to pass to ninja.

  Returns:
    List of the contents of the compile database.
  """
  # TODO(dcheng): Ensure that clang is enabled somehow.

  # First, generate the compile database.
  ninja_path = GetNinjaPath()
  if not os.path.exists(ninja_path):
    ninja_path = shutil.which('ninja')
  if targets is None:
    targets = []
  json_compile_db = subprocess.check_output(
      [ninja_path, '-C', path] + targets +
      ['-t', 'compdb', 'cc', 'cxx', 'objc', 'objcxx'])
  return json.loads(json_compile_db)


def Read(path):
  """Reads a compile database into memory.

  Args:
    path: Directory that contains the compile database.
  """
  with open(os.path.join(path, 'compile_commands.json'), 'rb') as db:
    return json.load(db)

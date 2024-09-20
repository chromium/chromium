#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate sample ChromeOS OOBE screen with a given name.

Screen will implement all needed interfaces on the C++ side and will be included
in the corresponding BUILD files.

Script will generate sample TS and HTML files that user will be able to display
in the OOBE's WebView. Corresponding BUILD files will be updated.

If --no-webview flag is specified previous step will be skipped.

After generation is complete user still needs to manually insert screen into the
OOBE's wizard controller flow and add WebView content to the screens.ts file.

usage: generate_screen_template.py [-h] [--no-webview] name

positional arguments:
  name          Name of a new screen. Expected to be in the camel case
                (e.g. MarketingOptIn). No "Screen" suffix is needed.

options:
  -h, --help    show this help message and exit
  --no-webview  Indicates whether user wants to skip creation of TS/HTML files.
                (default: False)
"""

import argparse
import logging
import os
import re
import subprocess
import sys

# Configure logging with timestamp and log level.
logging.basicConfig(level=logging.INFO,
                    format="[%(asctime)s:%(levelname)s] %(message)s")

if __name__ == '__main__':
  # Need to add directories with other scripts to the PATH variable to be able
  # to use them inside the tools/oobe folder.
  sys.path.append(os.path.abspath(os.path.join(sys.path[0], '..')))
import sort_sources


def GetGitRoot() -> str:
  """Retrieves absolute path to the repository root.

  Returns:
    An absolute path to the repository root in utf-8.
  """
  job = subprocess.Popen(['git', 'rev-parse', '--show-toplevel'],
                         stdout=subprocess.PIPE)
  out, err = job.communicate()
  if job.returncode != 0:
    logging.error('Error getting a git root: %s', err.decode('utf-8'))
    sys.exit(1)
  return out.decode('utf-8').strip()


GIT_ROOT = GetGitRoot()
SCREEN_GN_FILE_PATH = os.path.join(GIT_ROOT,
                                   'chrome/browser/ash/login/screens/BUILD.gn')
HANDLER_GN_FILE_PATH = os.path.join(
    GIT_ROOT, 'chrome/browser/ui/webui/ash/login/BUILD.gn')
WEBVIEW_TS_LIBRARY_GN_FILE_PATH = os.path.join(
    GIT_ROOT, 'chrome/browser/resources/chromeos/login/screens/common/BUILD.gn')
WEBVIEW_GN_FILE_PATH = os.path.join(
    GIT_ROOT, 'chrome/browser/resources/chromeos/login/BUILD.gn')


def IsCamelCase(screen_name: str) -> bool:
  """Checks whether new screen's name is in the camel case.

  Args:
    screen_name: A string that represents new screen name.

  Returns:
    A boolean value that represents whether given screen name is in the camel
    case.
  """
  return re.match('^([A-Z][a-z]+)+$', screen_name)


def GetScreenNameWords(screen_name: str) -> list[str]:
  """Breaks camel-case screen name into the words.

  Splits given camel case string into the logical lexems thath will be used to
  substitute new screen's name inside the generated C++/TS files.

  Args:
    screen_name: A string that represents new screen name in camel case.

  Returns:
    An array of strings that represent logical lexems in a given camel case
    screen name.
  """
  return re.sub('([A-Z][a-z]+)', r' \1', re.sub('([A-Z]+)', r' \1',
                                                screen_name)).split()


def FindAndReplace(name_words: list[str], file_path: str):
  """Applies set of replace rules to a file.

  Reads content of a file from a given path and applies set of replace rules
  to it's content. Overrides file content in the end.

  Args:
    name_words: An array of strings that represents logical lexems of a new
      screen's name.
    file_path: An absolute path to a file.
  """
  rules = [
      # Update placeholder class camel case name to the corresponding new
      # screen's name. E.g. `class PlaceholderScreen`` will become
      # `class MyNewScreen`.`
      lambda content: re.sub('Placeholder', ''.join(name_words), content),

      # Update include guards.
      lambda content: re.sub('PLACEHOLDER', '_'.join(
          word.upper() for word in name_words), content),

      # Update include paths for both screen and handler. This simple substitute
      # should be sufficient because new CPP files will be in the same folder
      # with their copies.
      lambda content: re.sub('placeholder_screen', ('_'.join(
          word.lower() for word in name_words) + '_screen'), content),

      # Update screen id inside the handler and element id inside the TS file.
      lambda content: re.sub('placeholder', '-'.join(
          word.lower() for word in name_words), content)
  ]

  with open(file_path, encoding='utf-8') as f:
    content = f.read()
    for rule in rules:
      content = rule(content)

  with open(file_path, mode='w', encoding='utf-8', newline='\n') as f:
    f.write(content)

  logging.info('Updated source file: %s', file_path)


def UpdateGnFile(name_words: list[str], gn_file_path: str):
  """Updates given GN file with the new include rules.

  Tries to find one of the RegEx rules in each line of the GN file and inserts
  new lines right after it. Sorts sources of the update GN file.

  Args:
    name_words: An array of strings that represents logical lexems of a new
      screen's name.
    gn_file_path: An absolute path to a GN file.
  """
  with open(gn_file_path, encoding='utf-8') as f:
    content = f.readlines()

  result = []
  for line in content:
    result.append(line)
    is_cpp_include = re.search(r'placeholder_screen.*\.h', line)
    is_ts_include = re.search(r'placeholder.ts', line)
    if is_cpp_include or is_ts_include:
      new_file = re.sub('placeholder',
                        '_'.join(word.lower() for word in name_words), line)
      if is_cpp_include:
        result.append(re.sub('\.h', '.cc', new_file))
      result.append(new_file)

  with open(gn_file_path, mode='w', encoding='utf-8', newline='\n') as f:
    f.writelines(result)

  # Currently, works only with the CPP includes.
  sort_sources.ProcessFile(gn_file_path, should_confirm=False)

  logging.info('Updated GN file: %s', gn_file_path)


def FindPlacholderFilesPaths(globs: list[str]) -> list[str]:
  """Finds placeholder files inside git repository with the git grep.

  Args:
    globs: List of strings that represents expected file extensions.

  Returns:
    An array of string paths to the placeholder files.

  Raises:
    Exception if subprocess failed.
  """
  job = subprocess.Popen(
      ['git', 'grep', '-E', '--name-only', 'PlaceholderScreen', '--'] + globs,
      stdout=subprocess.PIPE,
      cwd=GIT_ROOT)
  out, err = job.communicate()
  if job.returncode != 0:
    raise Exception(f'Error executing git grep: {err.decode("utf-8")}')

  return [path.decode('utf-8') for path in out.splitlines()]


def CopyPlaceholderFile(path: str, new_file_path: str):
  """Executes cp command to create a copy of the placeholder file.

  Args:
    path: Path to the original plachoolder file.
    new_file_path: Path to a new file.

  Raises:
    Exception if subprocess failed.
  """
  job = subprocess.Popen(['cp', path, new_file_path], cwd=GIT_ROOT)
  _, err = job.communicate()
  if job.returncode != 0:
    raise Exception(f'Error copying files: {err.decode("utf-8")}')

  logging.info('Created new file: %s', new_file_path)


def GenerateCppFiles(name_words: list[str]):
  """Generates header and source files for screen and handler classes.

  This function does the following operations:
    1) Copy content of the PlaceholderScreen class, which implements all needed
       intefaces, into a new files with a given screen name.
    2) Do a series of find and replace operations to move
       includes/header guards/class names/etc to a new name.
    3) Add new files to the corresponding BUILD.gn files, sort build list to
       preserve a right order.

  Args:
    name_words: An array of strings that represents logical lexems of a new
      screen's name.
  """
  cpp_globs = ['*.cc', '*.h']
  paths = FindPlacholderFilesPaths(cpp_globs)
  assert (
      len(paths) == 4
  ), 'There should be only 4 CPP files to copy (screen/handler .h and .cc)'

  for path in paths:
    new_file_path = re.sub('placeholder',
                           '_'.join(s.lower() for s in name_words), path)

    CopyPlaceholderFile(path, new_file_path)

    FindAndReplace(name_words, os.path.join(GIT_ROOT, new_file_path))

  cpp_gn_files = [SCREEN_GN_FILE_PATH, HANDLER_GN_FILE_PATH]
  for name in cpp_gn_files:
    UpdateGnFile(name_words, name)


def GenerateWebviewFiles(name_words: list[str]):
  """Generates TS and HTML files for a new screen.

  This function does the following operations:
    1) Copy content of the placeholder.ts and placeholder.html files
    2) Do a find and replace operations over new TS file.
    3) Add TS file to the corresponding BUILD.gn file.

  TS BUILD.gn rules currently will not be sorted, as sort_sources doesn't
  support those extensions.

  Args:
    name_words: An array of strings that represents logical lexems of a new
      screen's name.
  """
  webview_globs = ['*.ts']
  paths = FindPlacholderFilesPaths(webview_globs)

  assert (len(paths) == 1), 'Only 1 TS file should be found'

  # HTML file doesn't contain any words that could be used verify it's
  # correctness, so we can generate it's path from the corresponding TS file.
  paths.append(re.sub(r'\.ts', '.html', paths[0]))

  for path in paths:
    new_file_path = re.sub('placeholder',
                           '_'.join(s.lower() for s in name_words), path)

    CopyPlaceholderFile(path, new_file_path)

    if new_file_path[-3:] == '.ts':
      FindAndReplace(name_words, os.path.join(GIT_ROOT, new_file_path))

  UpdateGnFile(name_words, WEBVIEW_GN_FILE_PATH)


def GenerateScreen(name_words: list[str], no_webview: bool):
  """Executes new screen generation flow.

  Args:
    name_words: An array of strings that represents logical lexems of a new
      screen's name.
    no_webview: True if WebView files should not be generated.
  """
  logging.info('Generating screen and handler files')
  GenerateCppFiles(name_words)
  if no_webview:
    logging.info('--no-webview flag is specified, skipping WebView generation')
    return
  logging.info('Generating WebView files')
  GenerateWebviewFiles(name_words)


def main():
  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument(
      'name',
      help="""Name of a new screen. Expected to be in the camel case
      (e.g. MarketingOptIn). No "Screen" suffix is needed.""")
  parser.add_argument(
      '--no-webview',
      help='Indicates whether user wants to skip creation of TS/HTML files.',
      action='store_true')

  options = parser.parse_args()
  if not IsCamelCase(options.name):
    logging.error('Screen name must be in the camel case')
    parser.print_help()
    return 1
  screen_name_words = GetScreenNameWords(options.name)
  if screen_name_words[-1].lower() == 'screen':
    logging.error(
        '"Screen" suffix is not needed, it will be added automatically')
    parser.print_help()
    return 1

  # Catching all kinds of errors here to generate non-zero exit code.
  # This might be used in future to add a test to the CQ to verify that script
  # works properly.
  try:
    GenerateScreen(screen_name_words, options.no_webview)
  except Exception as ex:
    logging.error(str(ex))
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())

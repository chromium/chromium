# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Given a path to a directory of CodeQL logs, this script analyzes the
contents of those logs to determine whether the CodeQL extractor encountered
any errors that affect the integrity of the finalized database.

On success (database is intact), this script returns 0.

On failure (database is corrupt OR has unknown status), this script returns -1
and outputs a list of the specific logfiles that correspond to integrity errors,
plus a list of the specific logfiles containing unknown errors."""
import os
import argparse

# The set of errors that are known to exist, but do not affect the integrity
# of the finalized database.
KNOWN_INCONSEQUENTIAL_ERRORS = [
    "Unknown expr kind 30",
    "Unexpected template kind 9",
    "Unknown expr kind 31",
    "Unknown expr kind 34",
    "Unexpected dynamic init kind 1",
    "Unexpected dynamic init kind 2",
    "Unexpected dynamic init kind 3",
    "Unexpected dynamic init kind 6",
    "Unexpected dynamic init kind 7",
    "Unknown kind 5",
    ("In fabricate_destructors_expr: Unsupported expression kind encountered "
     "while fabricating destructors (fabricate_destructors_expr, 31)."),
]

# TODO(flowerhack): These error messages are pretty brittle at the moment. As we
# get a better understanding of which errors are problematic vs which aren't,
# update these to match the appropriate errors more flexible (e.g. remove
# references to specific files, etc)
KNOWN_PROBLEMATIC_ERRORS = [
    ('In construct_text_message: "../../third_party/dawn/src/dawn/native/'
     'webgpu_absl_format.cpp", line 118: internal error: assertion failed: '
     'cast_node: cast to class type (exprutil.c, line 9462 in cast_node)'),
    ('Warning[extractor-c++]: In construct_text_message: "../../base/functional'
     '/function_ref.h", line 69: internal error: assertion failed at: '
     '"decls.c", line 21165 in mark_decl_after_first_in_comma_list')
]

GENERIC_EXTRACTOR_ERROR = ('Warning[extractor-c++]: In main: Extractor exiting '
                           'with code 1')


def line_in_list(line, errlist):
  """ For a given line, checks if it matches any error in errlist. """
  return any([line in error for error in errlist])


def check_if_log_lines_indicate_nontrivial_error(
    filename, log_lines, extractor_logs_indicating_integrity_errors,
    extractor_logs_containing_unknown_errors):
  """ For a given list of log_lines, checks if any of them (1) known problematic
  errors or (2) unknown errors, and stores the filename in the appropriate
  list if so."""
  for line in log_lines:
    # Check if the warnings are problematic.
    if "Warning[extractor-c++]:" not in line:
      continue
    if line_in_list(line, KNOWN_INCONSEQUENTIAL_ERRORS):
      pass
    elif line_in_list(line, KNOWN_PROBLEMATIC_ERRORS):
      extractor_logs_indicating_integrity_errors.append(filename)
      return
    else:
      extractor_logs_containing_unknown_errors.append(filename)
      return


def main():
  parser = argparse.ArgumentParser(
      description='Parse arguments for validation of CodeQL databases')
  parser.add_argument(
      '--codeql_log_path',
      '-l',
      type=str,
      required=True,
      help='Absolute path of a CodeQL log directory (e.g. "/codeql_db/logs")')
  args = parser.parse_args()
  logs_directory_path = os.path.abspath(os.path.expanduser(
      args.codeql_log_path))
  extractor_logs_directory_path = os.path.join(logs_directory_path, 'extractor')

  extractor_logs_indicating_integrity_errors = []
  extractor_logs_containing_unknown_errors = []

  # The logs in extractor/ are deeply nested; `os.walk` gets us the complete
  # list of logfiles.
  num_files_scanned = 0
  num_files_with_errors = 0
  for root, dirs, files in os.walk(extractor_logs_directory_path):
    for filename in files:
      num_files_scanned += 1
      filename = os.path.join(root, filename)

      # Check the last line of the log to see if the extractor existed with a
      # non-zero status.
      with open(filename, 'r') as f:
        lines = f.read().splitlines()
        if not GENERIC_EXTRACTOR_ERROR in lines[-1]:
          continue
        # If so, scan all errors emanated by the extractor and classify them.
        num_files_with_errors += 1
        check_if_log_lines_indicate_nontrivial_error(
            filename, lines, extractor_logs_indicating_integrity_errors,
            extractor_logs_containing_unknown_errors)

  if not (extractor_logs_indicating_integrity_errors
          or extractor_logs_containing_unknown_errors):
    print("Database contains no integrity errors.")
    return 0

  print("A problem was detected with the database.")
  print("Paths of logfiles that indicate integrity errors:")
  for filepath in extractor_logs_indicating_integrity_errors:
    print(filepath)
  print("Paths of logfiles containing unknown errors:")
  for filepath in extractor_logs_containing_unknown_errors:
    print(filepath)
  print(f"{num_files_with_errors}/{num_files_scanned}"
        " files contained errors")
  print(f"{len(extractor_logs_containing_unknown_errors)} unknown errors")
  print(f"{len(extractor_logs_indicating_integrity_errors)} integrity errors")
  return -1


if __name__ == '__main__':
  exit(main())

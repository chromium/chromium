#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs traffic_annotation_auditor on the given change list or all files to make
sure network traffic annoations are syntactically and semantically correct and
all required functions are annotated.
"""

from __future__ import print_function

import os
import argparse
import sys

from annotation_tools import NetworkTrafficAnnotationTools

# If this test starts failing, please set TEST_IS_ENABLED to "False" and file a
# bug to get this reenabled, and cc the people listed in
# //tools/traffic_annotation/OWNERS.
TEST_IS_ENABLED = True

# Threshold for the change list size to trigger full test.
CHANGELIST_SIZE_TO_TRIGGER_FULL_TEST = 100


class NetworkTrafficAnnotationChecker():
  EXTENSIONS = ['.cc', '.mm',]
  ANNOTATIONS_FILE = 'annotations.xml'

  def __init__(self, build_path=None):
    """Initializes a NetworkTrafficAnnotationChecker object.

    Args:
      build_path: str Absolute or relative path to a fully compiled build
          directory. If not specified, the script tries to find it based on
          relative position of this file (src/tools/traffic_annotation).
    """
    self.tools = NetworkTrafficAnnotationTools(build_path)

  def IsAnnotationsFile(self, file_path):
    """Returns true if the given file is the annotations file."""
    return os.path.basename(file_path) == self.ANNOTATIONS_FILE

  def ShouldCheckFile(self, file_path):
    """Returns true if the input file has an extension relevant to network
    traffic annotations."""
    return os.path.splitext(file_path)[1] in self.EXTENSIONS

  def GetFilePaths(self, complete_run, limit):
    if complete_run:
      return []

    # Get list of modified files. If failed, silently ignore as the test is
    # run in error resilient mode.
    file_paths = self.tools.GetModifiedFiles() or []

    annotations_file_changed = any(
        self.IsAnnotationsFile(file_path) for file_path in file_paths)

    # If the annotations file has changed, trigger a full test to avoid
    # missing a case where the annotations file has changed, but not the
    # corresponding file, causing a mismatch that is not detected by just
    # checking the changed .cc and .mm files.
    if annotations_file_changed:
      return []

    file_paths = [
        file_path for file_path in file_paths if self.ShouldCheckFile(
            file_path)]
    if not file_paths:
      return None

    # If the number of changed files in the CL exceeds a threshold, trigger
    # full test to avoid sending very long list of arguments and possible
    # failure in argument buffers.
    if len(file_paths) > CHANGELIST_SIZE_TO_TRIGGER_FULL_TEST:
      file_paths = []

    return file_paths

  def CheckFiles(self, complete_run, limit):
    """Passes all given files to traffic_annotation_auditor to be checked for
    possible violations of network traffic annotation rules.

    Args:
      complete_run: bool Flag requesting to run test on all relevant files.
      limit: int The upper threshold for number of errors and warnings. Use 0
          for unlimited.

    Returns:
      int Exit code of the network traffic annotation auditor.
    """
    if not self.tools.CanRunAuditor():
      print("Network traffic annotation presubmit check was not performed. A "
            "compiled build directory and traffic_annotation_auditor binary "
            "are required to do it.")
      return 0

    file_paths = self.GetFilePaths(complete_run, limit)
    if file_paths is None:
      return 0

    args = ["--test-only", "--limit=%i" % limit, "--error-resilient"] + \
           file_paths

    stdout_text, stderr_text, return_code = self.tools.RunAuditor(args)

    if stdout_text:
      print(stdout_text)
    if stderr_text:
      print("\n[Runtime Messages]:\n%s" % stderr_text)
    return return_code


def main():
  if not TEST_IS_ENABLED:
    return 0

  parser = argparse.ArgumentParser(
      description="Network Traffic Annotation Presubmit checker.")
  parser.add_argument(
      '--build-path',
      help='Specifies a compiled build directory, e.g. out/Debug. If not '
           'specified, the script tries to guess it. Will not proceed if not '
           'found.')
  parser.add_argument(
      '--limit', default=5,
      help='Limit for the maximum number of returned errors and warnings. '
           'Default value is 5, use 0 for unlimited.')
  parser.add_argument(
      '--complete', action='store_true',
      help='Run the test on the complete repository. Otherwise only the '
           'modified files are tested.')

  args = parser.parse_args()
  checker = NetworkTrafficAnnotationChecker(args.build_path)
  return checker.CheckFiles(args.complete, args.limit)


if '__main__' == __name__:
  sys.exit(main())

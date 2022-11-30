# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit tests for weblayer.

Used to verify incompatible changes have not been done to AIDL files.
"""

import glob
import logging
import os
import shutil
import subprocess
import sys
import tempfile

USE_PYTHON3 = True

_INCOMPATIBLE_API_ERROR_STRING = """You have made an incompatible API change.
Generally this means one of the following:
  A function has been removed.
  The arguments of a function has changed.
This tool also reports renames as errors, which are generally okay.
If the API you are changing was added in the current release, you can
safely ignore this warning."""

class AidlFile:
  """Provides information about an aidl file in the repo."""
  def __init__(self, f):
    # The AffectedFile.
    self.affected_file = f
    # Path of the aidl file in the repo, this is the full absolute path.
    self.path_in_repo = f.AbsoluteLocalPath()
    # Absolute path to where java files start.
    self.java_root_dir = ''
    # Package names of the file.
    self.packages = []
    # Name part of the file, e.g. IBrowser.aidl.
    self.file_name = os.path.basename(self.path_in_repo)
    current_dir = self.path_in_repo
    packages = []
    while current_dir != '':
      dir_name, base_name = os.path.split(current_dir)
      packages.append(base_name)
      if base_name == 'org':
        packages.reverse()
        self.java_root_dir = dir_name
        # Last item is the file name
        packages.pop()
        self.packages = packages
        break
      parent_dir = dir_name
      if current_dir == parent_dir:
        logging.warn('Unable to find file system root for %s',
                     self.path_in_repo)
        break
      current_dir = parent_dir

  def IsValid(self):
    """Returns true if this is a valid aidl file."""
    return len(self.packages) > 0

  def GetPathRelativeTo(self, other_dir):
    """Returns the path of the file relative to another directory.

    The path is built using the java packages.
    """
    return os.path.join(os.path.join(other_dir, *self.packages), self.file_name)


def _CompareApiDumpForFiles(input_api, output_api, aidl_files):
  if len(aidl_files) == 0:
    return []
  # These tests fail to run on Windows (devil_chromium needs some non-standard
  # Windows modules) and given the Android dependencies this is reasonable.
  if input_api.is_windows:
    return []

  repo_root = input_api.change.RepositoryRoot()
  build_android_dir = os.path.join(repo_root, 'build', 'android')
  sys.path.append(build_android_dir)
  sys.path.append(os.path.join(repo_root, 'third_party', 'catapult', 'devil'))
  import devil_chromium
  from devil.android.sdk import build_tools
  devil_chromium.Initialize()

  try:
    aidl_tool_path = build_tools.GetPath('aidl')
  except Exception as e:
    if input_api.no_diffs:
      # If we are running presubmits with --all or --files and the 'aidl' tool
      # cannot be found then that probably means that target_os = 'android' is
      # missing from .gclient and the failure is not interesting.
      return []
  if not os.path.exists(aidl_tool_path):
    return [output_api.PresubmitError(
        'Android sdk does not contain aidl command ' + aidl_tool_path)]

  framework_aidl_path = glob.glob(os.path.join(
      input_api.change.RepositoryRoot(), 'third_party', 'android_sdk',
      'public', 'platforms', '*', 'framework.aidl'))[0]
  logging.debug('Using framework.aidl at path %s', framework_aidl_path)

  tmp_old_contents_dir = tempfile.mkdtemp()
  tmp_old_aidl_dir = tempfile.mkdtemp()
  tmp_new_aidl_dir = tempfile.mkdtemp()
  aidl_src_dir = aidl_files[0].java_root_dir
  generate_old_api_cmd = [aidl_tool_path, '--dumpapi', '--out',
                          tmp_old_aidl_dir,
                          ('-p' + framework_aidl_path),
                          ('-I' + aidl_src_dir)]
  generate_new_api_cmd = [aidl_tool_path, '--dumpapi', '--out',
                          tmp_new_aidl_dir,
                          ('-p' + framework_aidl_path),
                          ('-I' + aidl_src_dir)]

  # The following generates the aidl api dump (both original and new) for any
  # changed file. The dumps are then compared using --checkapi.
  try:
    valid_file = False
    for aidl_file in aidl_files:
      old_contents = '\n'.join(aidl_file.affected_file.OldContents())
      if len(old_contents) == 0:
        # When |old_contents| is empty it indicates a new file, which is
        # implicitly compatible.
        continue
      old_contents_file_path = aidl_file.GetPathRelativeTo(tmp_old_contents_dir)
      # aidl expects the directory to match the java package names.
      if not os.path.isdir(os.path.dirname(old_contents_file_path)):
        os.makedirs(os.path.dirname(old_contents_file_path))
      with open(old_contents_file_path, 'w') as old_contents_file:
        old_contents_file.write(old_contents)
      generate_old_api_cmd += [old_contents_file_path]
      generate_new_api_cmd += [aidl_file.path_in_repo]
      valid_file = True

    if not valid_file:
      return []

    logging.debug('Generating old api %s', generate_old_api_cmd)
    result = subprocess.call(generate_old_api_cmd)
    if result != 0:
      return [output_api.PresubmitError('Error generating old aidl api dump')]

    logging.debug('Generating new api %s', generate_new_api_cmd)
    result = subprocess.call(generate_new_api_cmd)
    if result != 0:
      return [output_api.PresubmitError('Error generating new aidl api')]

    logging.debug('Diffing api')
    result = subprocess.call([aidl_tool_path, '--checkapi',
                              tmp_old_aidl_dir, tmp_new_aidl_dir])
    if result != 0:
      return [output_api.PresubmitPromptWarning(_INCOMPATIBLE_API_ERROR_STRING)]
  finally:
    shutil.rmtree(tmp_old_contents_dir)
    shutil.rmtree(tmp_old_aidl_dir)
    shutil.rmtree(tmp_new_aidl_dir)
  return []


def CheckChangeOnUpload(input_api, output_api):
  filter_lambda = lambda x: input_api.FilterSourceFile(
      x, files_to_check=[r'.*\.aidl$' ])
  aidl_files = []
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=filter_lambda):
    aidl_file = AidlFile(f)
    logging.debug('Possible aidl file %s', f.AbsoluteLocalPath())
    if aidl_file.IsValid():
      aidl_files.append(aidl_file)
    else:
      logging.warn('File matched aidl extension, but not valid ' +
                   f.AbsoluteLocalPath())
  if len(aidl_files) == 0:
    return []
  return _CompareApiDumpForFiles(input_api, output_api, aidl_files)

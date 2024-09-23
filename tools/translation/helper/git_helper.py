# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys
import os

if sys.platform.startswith('win'):
  # Use the |git.bat| in the depot_tools/ on Windows.
  GIT = 'git.bat'
else:
  GIT = 'git'


def list_grds_in_repository(repo_path):
  """Returns a list of all the grd files in the current git repository."""
  # This works because git does its own glob expansion even though there is no
  # shell to do it.
  # TODO(meacer): This should use list_grds_in_repository() from the internal
  #               translate.py.
  if os.getcwd().startswith('/google/cog/cloud'):
    files = []
    for _, _, filenames in os.walk(repo_path):
      files.extend([f for f in filenames if f.endswith('.grd')])
    return files
  output = subprocess.check_output([GIT, 'ls-files', '--', '*.grd'],
                                   cwd=repo_path)
  # Need to decode because Python3 returns subprocess output as bytes.
  return output.decode('utf8').strip().splitlines()


def git_add(files, repo_root):
  """Adds relative paths given in files to the current CL."""
  # Upload in batches in order to not exceed command line length limit.
  BATCH_SIZE = 50
  added_count = 0
  while added_count < len(files):
    batch = files[added_count:added_count + BATCH_SIZE]
    command = [GIT, 'add'] + batch
    subprocess.check_call(command, cwd=repo_root)
    added_count += len(batch)

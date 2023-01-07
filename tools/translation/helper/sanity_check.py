# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sanity checking for grd_helper.py. Run manually before uploading a CL."""

import io
import os
import subprocess
import sys

# Add the parent dir so that we can import from "helper".
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import grd_helper
from helper import translation_helper

if sys.platform.startswith('win'):
  # Use the |git.bat| in the depot_tools/ on Windows.
  GIT = 'git.bat'
else:
  GIT = 'git'

here = os.path.dirname(os.path.realpath(__file__))
repo_root = os.path.normpath(os.path.join(here, '..', '..', '..'))


def list_files_in_repository(repo_path, pattern):
  """Lists all files matching given pattern in the given git repository"""
  # This works because git does its own glob expansion even though there is no
  # shell to do it.
  output = subprocess.check_output([GIT, 'ls-files', '--', pattern],
                                   cwd=repo_path).decode('utf-8')
  return output.strip().splitlines()


def read_file_as_text(path):
  with io.open(path, mode='r', encoding='utf-8') as f:
    return f.read()


# Sanity checks to ensure that we can parse all grd and grdp files in the repo.
# Must not fail.
def Run():
  grds = list_files_in_repository(repo_root, '*.grd')
  grdps = list_files_in_repository(repo_root, '*.grdp')

  print('Found %d grds, %d grdps in the repo.' % (len(grds), len(grdps)))
  # Make sure we can parse all .grd files in the source tree. Grd files are
  # parsed via the file path.
  for grd in grds:
    # This file is intentionally missing an include, skip it.
    if grd == os.path.join('tools', 'translation', 'testdata', 'internal.grd'):
      continue
    path = os.path.join(repo_root, grd)
    grd_helper.GetGrdMessages(path, os.path.dirname(path))

  # Make sure we can parse all .grdp files in the source tree.
  # Grdp files are parsed using file contents instead of path.
  for grdp in grdps:
    path = os.path.join(repo_root, grdp)
    # Parse grdp files using file contents.
    contents = read_file_as_text(path)
    grd_helper.GetGrdpMessagesFromString(contents)

  print('Successfully parsed all .grd and .grdp files in the repo.')

  # Additional check for translateable grds. Translateable grds are a subset
  # of all grds so this checks some files twice, but it exercises the
  # get_translatable_grds() path and also doesn't need to skip internal.grd.
  TRANSLATION_EXPECTATIONS_PATH = os.path.join(repo_root, 'tools',
                                               'gritsettings',
                                               'translation_expectations.pyl')
  translateable_grds = translation_helper.get_translatable_grds(
      repo_root, grds, TRANSLATION_EXPECTATIONS_PATH)
  print('Found %d translateable .grd files in translation expectations.' %
        len(translateable_grds))
  for grd in translateable_grds:
    path = os.path.join(repo_root, grd.path)
    grd_helper.GetGrdMessages(path, os.path.dirname(path))
  print('Successfully parsed all translateable_grds .grd files in translation '
        'expectations.')
  print('DONE')


if __name__ == '__main__':
  Run()

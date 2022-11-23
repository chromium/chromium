#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to upload translation screenshots to Google Cloud Storage.

This tool searches the current repo for .png files associated with .grd or
.grdp files. It uploads the images to a Cloud Storage bucket and generates .sha1
files. Finally, it asks the user if they want to add the .sha1 files to their
CL.

Images must be named the same as the UI strings they represent
(e.g. IDS_HELLO.png for IDS_HELLO). The tool does NOT try to parse .grd/.grdp
files, so it doesn't know whether an image file corresponds to a message or not.
It will attempt to upload the image anyways.
"""

from __future__ import print_function
try:
  # In Python2, override input with raw_input for compatibility.
  input = raw_input  # pylint: disable=redefined-builtin
except NameError:
  pass

import argparse
import sys
import os
import subprocess

import helper.translation_helper as translation_helper
import helper.git_helper as git_helper

here = os.path.dirname(os.path.realpath(__file__))
src_path = os.path.normpath(os.path.join(here, '..', '..'))

depot_tools_path = os.path.normpath(
    os.path.join(src_path, 'third_party', 'depot_tools'))
sys.path.insert(0, depot_tools_path)

import upload_to_google_storage
import download_from_google_storage

sys.path.remove(depot_tools_path)

# Translation expectations file for the clank repo.
INTERNAL_TRANSLATION_EXPECTATIONS_PATH = os.path.join(
    'clank', 'tools', 'translation_expectations.pyl')

# Translation expectations file for the Chromium repo.
TRANSLATION_EXPECTATIONS_PATH = os.path.join('tools', 'gritsettings',
                                             'translation_expectations.pyl')

# URL of the bucket used for storing screenshots.
# This is writable by @google.com accounts, readable by everyone.
BUCKET_URL = 'gs://chromium-translation-screenshots'

if sys.platform.startswith('win'):
  # Use the |git.bat| in the depot_tools/ on Windows.
  GIT = 'git.bat'
else:
  GIT = 'git'


def query_yes_no(question, default='no'):
  """Ask a yes/no question via input() and return their answer.

  "question" is a string that is presented to the user.
  "default" is the presumed answer if the user just hits <Enter>.
      It must be "yes" (the default), "no" or None (meaning
      an answer is required of the user).

  The "answer" return value is True for "yes" or False for "no".
  """
  if default is None:
    prompt = '[y/n] '
  elif default == 'yes':
    prompt = '[Y/n] '
  elif default == 'no':
    prompt = '[y/N] '
  else:
    raise ValueError("invalid default answer: '%s'" % default)

  valid = {'yes': True, 'y': True, 'ye': True, 'no': False, 'n': False}
  while True:
    print(question, prompt)
    choice = input().lower()
    if default is not None and choice == '':
      return valid[default]
    if choice in valid:
      return valid[choice]
    print("Please respond with 'yes' or 'no' (or 'y' or 'n').")


def find_screenshots(repo_root, translation_expectations):
  """Returns a list of translation related .png files in the repository."""
  translatable_grds = translation_helper.get_translatable_grds(
      repo_root, git_helper.list_grds_in_repository(repo_root),
      translation_expectations)

  # Add the paths of grds and any files they include. This includes grdp files
  # and files included via <structure> elements.
  src_paths = []
  for grd in translatable_grds:
    src_paths.append(grd.path)
    src_paths.extend(grd.grdp_paths)
    src_paths.extend(grd.structure_paths)

  screenshots = []
  rename_to_lowercase_png = None
  for grd_path in src_paths:
    # Convert grd_path.grd to grd_path_grd/ directory.
    name, ext = os.path.splitext(os.path.basename(grd_path))
    relative_screenshots_dir = os.path.relpath(
        os.path.dirname(grd_path), repo_root)
    screenshots_dir = os.path.realpath(
        os.path.join(repo_root,
                     os.path.join(relative_screenshots_dir,
                                  name + ext.replace('.', '_'))))
    # Grab all the .png files under the screenshot directory. On a clean
    # checkout this should be an empty list, as the repo should only contain
    # .sha1 files of previously uploaded screenshots.
    if not os.path.exists(screenshots_dir):
      continue
    for f in os.listdir(screenshots_dir):
      if f in ('OWNERS', 'README.md', 'DIR_METADATA') or f.endswith('.sha1'):
        continue

      # Rename any files ending in .PNG to .png. File extensions on some
      # platforms are case-sensitive, so renaming to .png ensures that created
      # .png.sha1 files are the same type on all platforms.
      if f.endswith('.PNG'):
        if rename_to_lowercase_png is None:
          rename_to_lowercase_png = query_yes_no(
              '.PNG file(s) found, rename to .png for upload?')
        if rename_to_lowercase_png:
          f_path = os.path.join(screenshots_dir, f)
          f = os.path.splitext(f)[0] + '.png'
          f_path_lowercase_png = os.path.join(screenshots_dir, f)
          os.rename(f_path, f_path_lowercase_png)

      if not f.endswith('.png'):
        print('File with unexpected extension: %s in %s' % (f, screenshots_dir))
        continue
      screenshots.append(os.path.join(screenshots_dir, f))
  return screenshots


def main():
  parser = argparse.ArgumentParser(
      description='Upload translation screenshots to Google Cloud Storage')
  parser.add_argument(
      '-n',
      '--dry-run',
      action='store_true',
      help='Don\'t actually upload the images')
  parser.add_argument(
      '-c',
      '--clank_internal',
      action='store_true',
      help='Upload screenshots for strings in the downstream clank directory')
  args = parser.parse_args()
  if args.clank_internal:
    screenshots = find_screenshots(
        os.path.join(src_path, "clank"),
        os.path.join(src_path, INTERNAL_TRANSLATION_EXPECTATIONS_PATH))

  else:
    screenshots = find_screenshots(
        src_path, os.path.join(src_path, TRANSLATION_EXPECTATIONS_PATH))
  if not screenshots:
    print ("No screenshots found.\n\n"
           "- Screenshots must be located in the correct directory.\n"
           "  E.g. For IDS_HELLO_WORLD message in path/to/file.grd, save the "
           "screenshot at path/to/file_grd/IDS_HELLO_WORLD.png.\n"
           "- If you added a new, uncommitted .grd file, `git add` it so that "
           "this script can pick up its screenshot directory.")
    sys.exit(0)

  print('Found %d updated screenshot(s): ' % len(screenshots))
  for s in screenshots:
    print('  %s' % s)
  print()
  if not query_yes_no('Do you want to upload these to Google Cloud Storage?\n\n'
                      'FILES WILL BE VISIBLE TO A LARGE NUMBER OF PEOPLE. '
                      'DO NOT UPLOAD ANYTHING CONFIDENTIAL.'):
    sys.exit(0)

  # Creating a standard gsutil object, assuming there are depot_tools
  # and everything related is set up already.
  gsutil_path = os.path.abspath(os.path.join(depot_tools_path, 'gsutil.py'))
  gsutil = download_from_google_storage.Gsutil(gsutil_path, boto_path=None)

  if not args.dry_run:
    if upload_to_google_storage.upload_to_google_storage(
        input_filenames=screenshots,
        base_url=BUCKET_URL,
        gsutil=gsutil,
        force=False,
        use_md5=False,
        num_threads=10,
        skip_hashing=False,
        gzip=None) != 0:
      print ('Error uploading screenshots. Try running '
             '`download_from_google_storage --config`.')
      sys.exit(1)

  print()
  print('Images are uploaded and their signatures are calculated:')

  signatures = ['%s.sha1' % s for s in screenshots]
  for s in signatures:
    print('  %s' % s)
  print()

  # Always ask if the .sha1 files should be added to the CL, even if they are
  # already part of the CL. If the files are not modified, adding again is a
  # no-op.
  if not query_yes_no('Do you want to add these files to your CL?',
                      default='yes'):
    sys.exit(0)

  if not args.dry_run:
    git_helper.git_add(signatures, src_path)

  print('DONE.')


if __name__ == '__main__':
  main()

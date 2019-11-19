#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to update our local profiles (AFDO or orderfiles)

This uses profiles of Chrome, or orderfiles for linking, provided by our
friends from Chrome OS. Though the profiles are available externally,
the bucket they sit in is otherwise unreadable by non-Googlers. Gsutil
usage with this bucket is therefore quite awkward: you can't do anything
but `cp` certain files with an external account, and you can't even do
that if you're not yet authenticated.

No authentication is necessary if you pull these profiles directly over
https.
"""

from __future__ import print_function

import argparse
import contextlib
import os
import subprocess
import sys
import urllib2

GS_HTTP_URL = 'https://storage.googleapis.com'


def ReadUpToDateProfileName(newest_profile_name_path):
  with open(newest_profile_name_path) as f:
    return f.read().strip()


def ReadLocalProfileName(local_profile_name_path):
  try:
    with open(local_profile_name_path) as f:
      return f.read().strip()
  except IOError:
    # Assume it either didn't exist, or we couldn't read it. In either case, we
    # should probably grab a new profile (and, in doing so, make this file sane
    # again)
    return None


def WriteLocalProfileName(name, local_profile_name_path):
  with open(local_profile_name_path, 'w') as f:
    f.write(name)


def CheckCallOrExit(cmd):
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()
  exit_code = proc.wait()
  if not exit_code:
    return

  complaint_lines = [
      '## %s failed with exit code %d' % (cmd[0], exit_code),
      '## Full command: %s' % cmd,
      '## Stdout:\n' + stdout,
      '## Stderr:\n' + stderr,
  ]
  print('\n'.join(complaint_lines), file=sys.stderr)
  sys.exit(1)


def RetrieveProfile(desired_profile_name, out_path, gs_url_base):
  # vpython is > python 2.7.9, so we can expect urllib to validate HTTPS certs
  # properly.
  ext = os.path.splitext(desired_profile_name)[1]
  compressed_path = out_path + ext
  gs_prefix = 'gs://'
  if not desired_profile_name.startswith(gs_prefix):
    gs_url = os.path.join(GS_HTTP_URL, gs_url_base, desired_profile_name)
  else:
    gs_url = os.path.join(GS_HTTP_URL, desired_profile_name[len(gs_prefix):])

  with contextlib.closing(urllib2.urlopen(gs_url)) as u:
    with open(compressed_path, 'wb') as f:
      while True:
        buf = u.read(4096)
        if not buf:
          break
        f.write(buf)

  if ext == '.bz2':
    # NOTE: we can't use Python's bzip module, since it doesn't support
    # multi-stream bzip files. It will silently succeed and give us a garbage
    # profile.
    # bzip2 removes the compressed file on success.
    CheckCallOrExit(['bzip2', '-d', compressed_path])
  elif ext == '.xz':
    # ...And we can't use the `lzma` module, since it was introduced in python3.
    # xz removes the compressed file on success.
    CheckCallOrExit(['xz', '-d', compressed_path])
  else:
    # Wait until after downloading the file to check the file extension, so the
    # user has something usable locally if the file extension is unrecognized.
    raise ValueError(
        'Only bz2 and xz extensions are supported; "%s" is not' % ext)


def main():
  parser = argparse.ArgumentParser(
      'Downloads profile/orderfile provided by Chrome OS')

  parser.add_argument(
      '--newest_state',
      required=True,
      help='Path to the file with name of the newest profile. '
           'We use this file to track the name of the newest profile '
           'we should pull'
  )
  parser.add_argument(
      '--local_state',
      required=True,
      help='Path of the file storing name of the local profile. '
           'We use this file to track the most recent profile we\'ve '
           'successfully pulled.'
  )
  parser.add_argument(
      '--gs_url_base',
      required=True,
      help='The base GS URL to search for the profile.'
  )
  parser.add_argument(
      '--output_name',
      required=True,
      help='Output name of the downloaded and uncompressed profile.'
  )
  parser.add_argument(
      '-f', '--force',
      action='store_true',
      help='Fetch a profile even if the local one is current'
  )
  args = parser.parse_args()

  up_to_date_profile = ReadUpToDateProfileName(args.newest_state)
  if not args.force:
    local_profile_name = ReadLocalProfileName(args.local_state)
    # In a perfect world, the local profile should always exist if we
    # successfully read local_profile_name. If it's gone, though, the user
    # probably removed it as a way to get us to download it again.
    if local_profile_name == up_to_date_profile \
        and os.path.exists(args.output_name):
      return 0

  new_tmpfile = args.output_name + '.new'
  RetrieveProfile(up_to_date_profile, new_tmpfile, args.gs_url_base)
  os.rename(new_tmpfile, args.output_name)
  WriteLocalProfileName(up_to_date_profile, args.local_state)


if __name__ == '__main__':
  sys.exit(main())

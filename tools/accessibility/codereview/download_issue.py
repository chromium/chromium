#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Downloads a patch and changed files from Rietveld.

Prints the patch of the most recent patchset to stdout.
"""

try:
  import base64

  import gerrit_util
  import git_cl
  import optparse
  import os.path
  #  import StringIO
  import sys
  import tarfile
  #import urllib2

  from third_party import colorama
except ImportError as e:
  print(e)
  print('Perhaps you\'re missing depot_tools in your PYTHONPATH.')
  import sys
  sys.exit(1)


def Progress(message):
  print(message, file=sys.stderr)


def DieWithError(message):
  print(message, file=sys.stderr)
  sys.exit(1)


def main(argv):
  parser = optparse.OptionParser()
  parser.set_usage('%prog [options] issue_number')
  parser.description = __doc__.strip()
  options, args = parser.parse_args(argv)
  if len(args) != 1:
    parser.print_help()
    return 0

  change_id = ""
  try:
    issue = int(args[0])
  except ValueError:
    try:
      change_id = str(args[0])
    except ValueError:
      DieWithError('Invalid issue number or change id')

  if not change_id:
    HOST_ = "chromium-review.googlesource.com"
    change_id = gerrit_util.GetChange(HOST_, issue)["change_id"]
  else:
    HOST_ = "googleplex-android-review.git.corp.google.com"
  query = gerrit_util.GetChangeCurrentRevision(HOST_, change_id)[0]
  current_revision_id = query["current_revision"]
  current_revision = query["revisions"][current_revision_id]
  patchset = current_revision["_number"]
  ref = current_revision["ref"]

  # Fetch the current branch.
  Progress("Fetching... " + ref)
  git_cl.RunGit(
      ["fetch", "https://chromium.googlesource.com/chromium/src", ref])
  print('Issue: %d, patchset: %d\n' % (issue, patchset))
  print()
  print(git_cl.RunGit(["show", "FETCH_HEAD"]))
  git_cl.RunGit(["checkout", "FETCH_HEAD"])

  Progress("finished")
  Progress("Run git checkout FETCH_HEAD, to start reviewing.")


if __name__ == '__main__':
  # These affect sys.stdout so do it outside of main() to simplify mocks in
  # unit testing.

  colorama.init()
  sys.exit(main(sys.argv[1:]))

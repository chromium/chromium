# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Some utility methods for getting and manipulating paths."""

# TODO(pamg): Have the buildbot use these, too.


import errno
import os
import sys

class PathNotFound(Exception): pass

def ScriptDir():
  """Get the full path to the directory containing the current script."""
  script_filename = os.path.abspath(sys.argv[0])
  return os.path.dirname(script_filename)

def FindAncestor(start_dir, ancestor):
  """Finds an ancestor dir in a path.

  For example, FindAncestor('c:\foo\bar\baz', 'bar') would return
  'c:\foo\bar'.  Unlike FindUpward*, this only looks at direct path ancestors.
  """
  start_dir = os.path.abspath(start_dir)
  path = start_dir
  while True:
    (parent, tail) = os.path.split(path)
    if tail == ancestor:
      return path
    if not tail:
      break
    path = parent
  raise PathNotFound("Unable to find ancestor %s in %s" % (ancestor, start_dir))

def FindUpwardParent(start_dir, *desired_list):
  """Finds the desired object's parent, searching upward from the start_dir.

  Searches start_dir and all its parents looking for the desired directory
  or file, which may be given in one or more path components. Returns the
  first directory in which the top desired path component was found, or raises
  PathNotFound if it wasn't.
  """
  desired_path = os.path.join(*desired_list)
  last_dir = ''
  cur_dir = start_dir
  found_path = os.path.join(cur_dir, desired_path)
  while not os.path.exists(found_path):
    last_dir = cur_dir
    cur_dir = os.path.dirname(cur_dir)
    if last_dir == cur_dir:
      raise PathNotFound('Unable to find %s above %s' %
                         (desired_path, start_dir))
    found_path = os.path.join(cur_dir, desired_path)
  # Strip the entire original desired path from the end of the one found
  # and remove a trailing path separator, if present.
  found_path = found_path[:len(found_path) - len(desired_path)]
  if found_path.endswith(os.sep):
    found_path = found_path[:len(found_path) - 1]
  return found_path


def FindUpward(start_dir, *desired_list):
  """Returns a path to the desired directory or file, searching upward.

  Searches start_dir and all its parents looking for the desired directory
  or file, which may be given in one or more path components. Returns the full
  path to the desired object, or raises PathNotFound if it wasn't found.
  """
  parent = FindUpwardParent(start_dir, *desired_list)
  return os.path.join(parent, *desired_list)


def MaybeMakeDirectory(*path):
  """Creates an entire path, if it doesn't already exist."""
  file_path = os.path.join(*path)
  try:
    os.makedirs(file_path)
  except OSError as e:
    # errno.EEXIST is "File exists".  If we see another error, re-raise.
    if e.errno != errno.EEXIST:
      raise

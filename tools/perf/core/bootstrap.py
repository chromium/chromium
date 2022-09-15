# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def ListAllDepsPaths(deps_file):
  """Recursively returns a list of all paths indicated in this deps file.

  Note that this discards information about where path dependencies come from,
  so this is only useful in the context of a Chromium source checkout that has
  already fetched all dependencies.

  Args:
    deps_file: File containing deps information to be evaluated, in the
               format given in the header of this file.
  Returns:
    A list of string paths starting under src that are required by the
    given deps file, and all of its sub-dependencies. This amounts to
    the keys of the 'deps' dictionary.
  """
  chrome_root = os.path.dirname(__file__)
  while os.path.basename(chrome_root) != 'src':
    chrome_root = os.path.abspath(os.path.join(chrome_root, '..'))

  loaded = {}
  exec(open(deps_file).read(), globals(), loaded)  # pylint: disable=exec-used
  deps = loaded.get('deps', {})
  deps_includes = loaded.get('deps_includes', {})

  deps_paths = list(deps.keys())

  for path in deps_includes.keys():
    # Need to localize the paths.
    path = os.path.join(chrome_root, '..', path)
    deps_paths += ListAllDepsPaths(path)

  return deps_paths

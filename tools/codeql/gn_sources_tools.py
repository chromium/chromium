# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import subprocess
import multiprocessing
import functools

def _convert_gn_sources_list_to_dict(gn_sources_list, build_dir):
  """ Given a list of gn sources, transform them into standard filepaths and
  place them in a dictionary. """
  outpath_pattern = "//%s/" % build_dir
  gn_sources_dict = {}
  for line in gn_sources_list:
    fixedline = line.replace(outpath_pattern, "").replace("//",
                                                          "../../").strip()
    gn_sources_dict[fixedline] = True
  return gn_sources_dict


def _get_sources_for_gn_target(all_transitive_sources, gn_path, build_dir,
                               target_name):
  """ Given a particular target, stores all the source files for that target in
  the given multiprocessing.Manager().dict().

  Example input:
  Args:

    target_name: The name of a GN target e.g.
      '//base/allocator/partition_allocator:partition_alloc'
    all_transitive_sources: A multiprocess.Manager().dict().

  Returns:
    Nothing, but at the end of this function's execution, args[1] will look
    like:
    {
      '//base/allocator/.../atomic_ref_count.h': True,
      '//base/allocator/.../bit_cast.h': True,
      ...
    }"""
  if target_name is not None:
    get_sources_command = [gn_path, "desc", build_dir, target_name, "sources"]
    sources_output = subprocess.run(get_sources_command,
                                    check=False,
                                    capture_output=True)
    if sources_output.returncode != 0:
      # Some `gn desc` are expected to fail
      # (because there's no `sources` for them).
      pass
    else:
      for file in sources_output.stdout.decode(encoding='utf-8').split("\n"):
        all_transitive_sources[file] = True


def _fetch_all_transitive_sources_for_gn_target(gn_target, build_dir, gn_path):
  """Fetches a list of all transitive source dependencies for a GN target.

  For a given GN target and build directory, returns a list with all the
  *transitive sources* upon which that target depends.

  This list can be useful for constructing a CodeQL database, since that list
  will be the 'minimal set' of commands required to generate a database.

  Args:
    gn_target: The name of a GN target e.g.
      `//components:components_unittests`.
    build_dir: The relative path to a Chromium build directory e.g.
      `out/release`.
  Returns:
    A list of sources, for example:
    ['//base/allocator.../atomic_ref_count.h',
     '//base/allocator/.../partition_alloc_base/bit_cast.h',
     ...
     '//ui/platform_window/extensions/workspace_extension.cc',
     ...]
    """
  get_deps_command = [gn_path, "desc", build_dir, gn_target, "deps", "--all"]
  deps_output = subprocess.run(get_deps_command,
                               check=True,
                               capture_output=True)
  target_names = deps_output.stdout.decode(encoding='utf-8').split("\n")
  my_cpu_count = int(multiprocessing.cpu_count())
  all_transitive_sources = multiprocessing.Manager().dict()
  with multiprocessing.Pool(my_cpu_count) as p:
    p.map(
        functools.partial(_get_sources_for_gn_target, all_transitive_sources,
                          gn_path, build_dir), target_names)
  return all_transitive_sources.keys()


def dictionary_of_all_transitive_sources(gn_target, build_dir, gn_path):
  """Constructs a dictionary of all transitive GN source deps for a target.

  For a given GN target (e.g. `//components:components_unittests`) and the
  path to some build directory (e.g. `out/release`), outputs a list of all
  *transitive sources* for that GN target, in the form of a dictionary where
  each entry has the value True.

  Args:
    gn_target: The name of a GN target e.g.
      `//components:components_unittests`.
    build_dir: The relative path to a Chromium build directory e.g.
      `out/release`.
  Returns:
    A dictionary that maps sources to True, for example:

    {'//base/allocator.../atomic_ref_count.h': True,
     '//base/allocator/.../partition_alloc_base/bit_cast.h': True,
     ...
     '//ui/platform_window/extensions/workspace_extension.cc': True,
     ...}
  """
  gn_sources_list = _fetch_all_transitive_sources_for_gn_target(
      gn_target, build_dir, gn_path)
  return _convert_gn_sources_list_to_dict(gn_sources_list, build_dir)

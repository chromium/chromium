# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils for interacting with builders & builder props in src."""

import json
import logging
import pathlib

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]
# TODO(crbug.com/41492688): Support src-internal configs too. When this is done,
# ensure tools/utr/recipe.py is not using the public reclient instance
_BUILDER_PROP_DIRS = _SRC_DIR.joinpath('infra', 'config', 'generated',
                                       'builders')
_INTERNAL_BUILDER_PROP_DIRS = _SRC_DIR.joinpath('internal', 'infra', 'config',
                                                'generated', 'builders')


def find_builder_props(bucket_name, builder_name):
  """Finds the checked-in json props file for the builder.

  Args:
    bucket_name: Bucket name of the builder
    builder_name: Builder name of the builder

  Returns:
    Tuple of (Dict of the builder's input props, LUCI project of the builder).
      Both elements will be None if the builder wasn't found.
  """

  def _walk_props_dir(props_dir):
    matches = []
    # TODO(crbug.com/41492688): Allow bucket_name to be optional?
    for bucket_path in props_dir.iterdir():
      if not bucket_path.is_dir() or bucket_path.name != bucket_name:
        continue
      for builder_path in bucket_path.iterdir():
        if builder_path.name != builder_name:
          continue
        prop_file = builder_path.joinpath('properties.json')
        if not prop_file.exists():
          logging.warning(
              'Found generated dir for builder at %s, but no prop file?',
              builder_path)
          continue
        matches.append(prop_file)
    return matches

  project = 'chrome'
  possible_matches = []
  if _INTERNAL_BUILDER_PROP_DIRS.exists():
    possible_matches += _walk_props_dir(_INTERNAL_BUILDER_PROP_DIRS)

  public_matches = _walk_props_dir(_BUILDER_PROP_DIRS)
  if public_matches:
    project = 'chromium'
    possible_matches += public_matches

  if not possible_matches:
    logging.error(
        '[red]No prop file found for %s.%s. Are you sure you have the '
        'correct bucket and builder name?[/]', bucket_name, builder_name)
    if not _INTERNAL_BUILDER_PROP_DIRS.exists():
      logging.warning(
          'src-internal not detected in this checkout. Perhaps the builder '
          'is a "chrome" one, in which: case make sure to add src-internal to '
          "your checkout if a you're a Googler.")
    return None, None
  if len(possible_matches) > 1:
    logging.error('[red]Found multiple prop files for builder %s:[/]',
                  builder_name)
    for m in possible_matches:
      logging.error(m)
    return None, None

  prop_file = possible_matches[0]
  logging.debug('Found prop file %s', prop_file)
  with open(possible_matches[0]) as f:
    props = json.load(f)

  return props, project

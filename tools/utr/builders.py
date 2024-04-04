# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils for interacting with builders & builder props in src."""

import json
import logging
import pathlib

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]
# TODO(crbug.com/41492688): Support src-internal configs too.
_BUILDER_PROP_DIRS = _SRC_DIR.joinpath('infra', 'config', 'generated',
                                       'builders')


def find_builder_props(bucket_name, builder_name):
  """Finds the checked-in json props file for the builder.

  Args:
    bucket_name: Bucket name of the builder
    builder_name: Builder name of the builder

  Returns:
    Tuple of (Dict of the builder's input props, Swarming server the builder
      runs on). Both elements will be None if the builder wasn't found.
  """
  # TODO(crbug.com/41492688): Allow bucket_name to be optional?
  possible_matches = []
  for bucket_path in _BUILDER_PROP_DIRS.iterdir():
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
      possible_matches.append(prop_file)

  if not possible_matches:
    logging.error(
        '[red]No prop file found for %s.%s. Are you sure you have the '
        'correct bucket and builder name?[/]', bucket_name, builder_name)
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

  # TODO(crbug.com/41492688): Support src-internal configs too. Fow now, assume
  # chromium builders correlate to "chromium-swarm".
  swarming_server = 'chromium-swarm'

  return props, swarming_server

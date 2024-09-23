# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils for interacting with builders & builder props in src."""

import json
import logging
import pathlib
import subprocess

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]
# TODO(crbug.com/41492688): Support src-internal configs too. When this is done,
# ensure tools/utr/recipe.py is not using the public reclient instance
_BUILDER_PROP_DIRS = _SRC_DIR.joinpath('infra', 'config', 'generated',
                                       'builders')
_INTERNAL_BUILDER_PROP_DIRS = _SRC_DIR.joinpath('internal', 'infra', 'config',
                                                'generated', 'builders')


def find_builder_props(builder_name, bucket_name=None, project_name=None):
  """Finds the checked-in json props file for the builder.

  Args:
    builder_name: Builder name of the builder
    bucket_name: Bucket name of the builder
    project_name: Project name of the builder

  Returns:
    Tuple of (Dict of the builder's input props, LUCI project of the builder).
      Both elements will be None if the builder wasn't found.
  """

  def _walk_props_dir(props_dir):
    matches = []
    if not props_dir.exists():
      return matches
    for bucket_path in props_dir.iterdir():
      if not bucket_path.is_dir() or (bucket_name
                                      and bucket_path.name != bucket_name):
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

  possible_matches = []
  if not project_name or project_name == 'chrome':
    matches = _walk_props_dir(_INTERNAL_BUILDER_PROP_DIRS)
    if matches:
      project_name = 'chrome'
      possible_matches += matches

  if not project_name or project_name == 'chromium':
    matches = _walk_props_dir(_BUILDER_PROP_DIRS)
    if matches:
      project_name = 'chromium'
      possible_matches += matches

  if not possible_matches:
    # Try also fetching the props from buildbucket. This will give us needed
    # vals like recipe and builder-group name for builders that aren't
    # bootstrapped.
    if bucket_name and project_name:
      logging.info(
          'Prop file not found, attempting to fetch props from buildbucket.')
      props = fetch_props_from_buildbucket(builder_name, bucket_name,
                                           project_name)
      if props:
        return props, project_name
    logging.error(
        '[red]No props found. Are you sure you have the correct project '
        '("%s"), bucket ("%s"), and builder name ("%s")?[/]', project_name,
        bucket_name, builder_name)
    if not _INTERNAL_BUILDER_PROP_DIRS.exists():
      logging.warning(
          'src-internal not detected in this checkout. Perhaps the builder '
          'is a "chrome" one, in which: case make sure to add src-internal to '
          "your checkout if a you're a Googler.")
    return None, None
  if len(possible_matches) > 1:
    logging.error(
        '[red]Found multiple prop files for builder %s. Pass in a project '
        '("-p") and bucket name ("-B").[/]', builder_name)
    for m in possible_matches:
      logging.error(m)
    return None, None

  prop_file = possible_matches[0]
  logging.debug('Found prop file %s', prop_file)
  with open(possible_matches[0]) as f:
    props = json.load(f)

  return props, project_name


def fetch_props_from_buildbucket(builder_name, bucket_name, project_name):
  """Calls out to buildbucket for the input props for the given builder

  Args:
    builder_name: Builder name of the builder
    bucket_name: Bucket name of the builder
    project_name: Project name of the builder

  Returns:
    Dict of the builder's input props
  """
  input_json = {
      'id': {
          'project': project_name,
          'bucket': bucket_name,
          'builder': builder_name,
      }
  }
  cmd = [
      'luci-auth',
      'context',
      '--',
      'prpc',
      'call',
      'cr-buildbucket.appspot.com',
      'buildbucket.v2.Builders.GetBuilder',
  ]
  logging.debug('Running prpc:')
  logging.debug(' '.join(cmd))
  p = subprocess.run(cmd,
                     input=json.dumps(input_json),
                     text=True,
                     stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,
                     check=False)
  if p.returncode:
    logging.warning('Error fetching the build template from buildbucket')
    # Use the "basic_logger" here (and below) to avoid rich from coloring random
    # bits of the printed error.
    logging.getLogger('basic_logger').warning(p.stdout.strip())
    return None
  builder_info = json.loads(p.stdout)
  props_s = builder_info.get('config', {}).get('properties', '{}')
  return json.loads(props_s)

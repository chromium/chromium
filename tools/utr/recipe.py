# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for invoking recipes"""

import json
import logging
import pathlib
import subprocess
import tempfile

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]


class LegacyRunner:
  """Interface for running the UTR recipe via the legacy `recipes.py run` mode.

  TODO(crbug.com/326904531): Sometime in Q2 2024, a more modernized option for
  running recipes locally should be made available. This file can/should be
  updated to support and utilize that new mode if/when it's available.
  """

  UTR_RECIPE_NAME = 'chromium/universal_test_runner'

  def __init__(self,
               bundle_root_path,
               builder_props,
               bucket,
               builder,
               tests,
               skip_compile,
               skip_test,
               build_dir=None):
    """Constructor for LegacyRunner

    Args:
      bundle_root_path: pathlib.Path to the root of the recipe bundle
      builder_props: Dict containing the props for the builder to run as.
      bucket: Bucket name of the builder to run as.
      builder: Builder name of the builder to run as.
      tests: List of tests to run.
      skip_compile: If True, the UTR will only run the tests.
      skip_test: If True, the UTR will only compile.
      builder_dir: pathlib.Path to the build dir to build in. Will use the UTR's
          default otherwise if needed.
    """
    self._recipes_py = bundle_root_path.joinpath('recipes')
    assert self._recipes_py.exists()

    # Add UTR recipe props. Its schema is located at:
    # https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipes/chromium/universal_test_runner.proto
    input_props = builder_props.copy()
    input_props['checkout_path'] = str(_SRC_DIR.parent)
    input_props['test_names'] = tests
    if build_dir:
      input_props['build_dir'] = build_dir

    mode = 'RUN_TYPE_COMPILE_AND_RUN'
    assert not (skip_compile and skip_test)
    if skip_compile:
      mode = 'RUN_TYPE_RUN'
    elif skip_test:
      mode = 'RUN_TYPE_COMPILE'
    input_props['run_type'] = mode

    # Need to pretend we're an actual build for various builder look-ups in
    # the recipe.
    input_props['$recipe_engine/buildbucket'] = {
        'build': {
            'builder': {
                # Should be safe to hard-code to 'chromium' even if the current
                # checkout is on a release branch.
                'project': 'chromium',
                'bucket': bucket,
                'builder': builder,
            },
        },
    }
    self._input_props = input_props

  def run_recipe(self):
    """Runs the UTR recipe with the settings defined on the CLI.

    Returns:
      Tuple of (exit code, error message) of the `recipes.py` invocation.
    """
    with tempfile.TemporaryDirectory() as tmp_dir:
      output_path = pathlib.Path(tmp_dir).joinpath('out.json')
      cmd = [
          self._recipes_py,
          'run',
          '--output-result-json',
          output_path,
          '--properties-file',
          '-',  # '-' means read from stdin
          self.UTR_RECIPE_NAME,
      ]
      p = subprocess.Popen(cmd, stdin=subprocess.PIPE, text=True)
      p.communicate(input=json.dumps(self._input_props))

      # Try to pull out the summary markdown from the recipe run.
      failure_reason = None
      if not output_path.exists():
        logging.error('Recipe output json not found')
      else:
        try:
          with open(output_path) as f:
            output = json.load(f)
          failure_reason = output.get('failure', {}).get('humanReason')
          # TODO(crbug.com/41492688): Also pull out info about gclient/GN arg
          # mismatches, surface those as a Y/N prompt to the user, and re-run
          # if Y.
        except json.decoder.JSONDecodeError:
          logging.exception('Recipe output is invalid json')

      # TODO(crbug.com/41492688): Support better status message streaming. For
      # now, just use the recipe engine's overly-verbose stdout.
      return p.returncode, failure_reason

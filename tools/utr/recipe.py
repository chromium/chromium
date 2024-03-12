# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for invoking recipes"""

import asyncio
import json
import logging
import os
import pathlib
import shutil
import subprocess
import tempfile

import output_adapter

# Disable noisy asyncio logs.
logging.getLogger('asyncio').setLevel(logging.WARNING)

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]


def check_rdb_auth():
  """Checks that the user is logged in with resultdb."""
  rdb_path = shutil.which('rdb')
  if not rdb_path:
    logging.error("'rdb' binary not found. Is depot_tools not on PATH?")
    return False
  cmd = [rdb_path, 'auth-info']
  p = subprocess.run(cmd,
                     stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,
                     text=True)
  if p.returncode:
    logging.error('No rdb auth available:')
    logging.error(p.stdout.strip())
    logging.error("Please run 'rdb auth-login' to authenticate")
    return False
  return True


def get_yn_resp():
  prompt = 'Do you wish to proceed? Please enter Y/N to confirm: '
  resp = input(prompt).strip()
  if resp and resp.lower() == 'y':
    return True
  return False


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
               swarming_server,
               tests,
               skip_compile,
               skip_test,
               skip_prompts,
               build_dir=None):
    """Constructor for LegacyRunner

    Args:
      bundle_root_path: pathlib.Path to the root of the recipe bundle
      builder_props: Dict containing the props for the builder to run as.
      bucket: Bucket name of the builder to run as.
      builder: Builder name of the builder to run as.
      swarming_server: Swarming server the builder runs on.
      tests: List of tests to run.
      skip_compile: If True, the UTR will only run the tests.
      skip_test: If True, the UTR will only compile.
      skip_prompts: If True, skip Y/N prompts for warnings.
      builder_dir: pathlib.Path to the build dir to build in. Will use the UTR's
          default otherwise if needed.
    """
    self._recipes_py = bundle_root_path.joinpath('recipes')
    self._swarming_server = swarming_server
    self._skip_prompts = skip_prompts
    assert self._recipes_py.exists()

    # Add UTR recipe props. Its schema is located at:
    # https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipes/chromium/universal_test_runner.proto
    input_props = builder_props.copy()
    input_props['checkout_path'] = str(_SRC_DIR)
    input_props['$recipe_engine/path'] = {'cache_dir': str(_SRC_DIR.parent)}
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

  def _run(self, filter_stdout, additional_props=None):
    """Internal implementation of invoking `recipes.py run`.

    Args:
      filter_stdout: If True, filters noisy log output from the recipe.
      additional_props: Dict containing additional props to pass to the recipe.
    Returns:
      Tuple of
        exit code of the `recipes.py` invocation,
        error message of the `recipes.py` invocation,
        a dict of additional_props the recipe should be re-invoked with
    """
    input_props = self._input_props.copy()
    input_props.update(additional_props or {})
    with tempfile.TemporaryDirectory() as tmp_dir:

      # TODO(crbug.com/41492688): Support both chrome and chromium realms. Just
      # hard-code 'chromium' for now.
      # Put all results in "try" realms. "try" should be writable for most devs,
      # while other realms like "ci" likely aren't. "try" is generally where we
      # confine untested code, so it's the best fit for our results here.
      rdb_realm = 'chromium:try'

      output_path = pathlib.Path(tmp_dir).joinpath('out.json')
      rerun_props_path = pathlib.Path(tmp_dir).joinpath('rerun_props.json')
      input_props['output_properties_file'] = str(rerun_props_path)
      cmd = [
          'rdb',
          'stream',
          '-new',
          '-realm',
          rdb_realm,
          '--',
          self._recipes_py,
          'run',
          '--output-result-json',
          output_path,
          '--properties-file',
          '-',  # '-' means read from stdin
          self.UTR_RECIPE_NAME,
      ]
      env = os.environ.copy()
      # This env var is read by both the cas and swarming recipe modules to
      # determine where to upload/run things.
      env['SWARMING_SERVER'] = f'https://{self._swarming_server}.appspot.com'

      async def exec_recipe():
        proc = await asyncio.create_subprocess_exec(
            cmd[0],
            *cmd[1:],
            limit=1024 * 1024 * 128,  # 128 MiB: there can be massive lines
            env=env,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT)

        proc.stdin.write(json.dumps(input_props).encode('ascii'))
        proc.stdin.write_eof()
        if filter_stdout:
          adapter = output_adapter.LegacyOutputAdapter()
        else:
          adapter = output_adapter.PassthroughAdapter()
        while not proc.stdout.at_eof():
          try:
            line = await proc.stdout.readline()
            adapter.ProcessLine(line.decode('utf-8').strip('\n'))
          except ValueError as e:
            logging.exception(f'Failed to parse line from the recipe')
        await proc.wait()
        return proc.returncode

      returncode = asyncio.run(exec_recipe())

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

      # If this file exists, the recipe is signalling to us that there's an
      # issue, and that we need to re-run if we're sure we want to proceed.
      # The contents of the file are the properties we should re-run it with.
      rerun_props = None
      if rerun_props_path.exists():
        with open(rerun_props_path) as f:
          rerun_props = json.load(f)

      return returncode, failure_reason, rerun_props

  def run_recipe(self, filter_stdout=True):
    """Runs the UTR recipe with the settings defined on the CLI.

    Args:
      filter_stdout: If True, filters noisy log output from the recipe.
    Returns:
      Tuple of (exit code, error message) of the `recipes.py` invocation.
    """
    rerun_props = None
    # We might need to run the recipe a handful of times before we receive a
    # final result. Put a cap on the amount of re-runs though, just in case.
    for _ in range(10):
      exit_code, error_msg, rerun_props = self._run(filter_stdout, rerun_props)
      if not rerun_props:
        return exit_code, error_msg
      else:
        logging.warning('')
        logging.warning(error_msg)
        logging.warning('')
        if not self._skip_prompts:
          should_continue = get_yn_resp()
        else:
          logging.warning(
              'Proceeding despite the recipe warning due to the presence of '
              '"--force".')
          should_continue = True
        if not should_continue:
          return exit_code, 'User-aborted due to warning'
    return 1, 'Exceeded too many recipe re-runs'

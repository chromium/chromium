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
import sys
import tempfile

from collections import namedtuple
from rich import markdown
from rich import console

import output_adapter

# Disable some noisy logs.
logging.getLogger('asyncio').setLevel(logging.WARNING)
logging.getLogger('markdown_it').setLevel(logging.WARNING)

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]

_RECLIENT_CLI = _SRC_DIR.joinpath('buildtools', 'reclient_cfgs',
                                  'configure_reclient_cfgs.py')
_SISO_CLI = _SRC_DIR.joinpath('build', 'config', 'siso', 'configure_siso.py')
_DEFAULT_RBE_PROJECT = 'rbe-chrome-untrusted'

RerunOption = namedtuple('RerunOption', ['prompt', 'properties'])


def check_luci_context_auth():
  """Checks that the user is logged in with luci-auth context."""
  luci_auth_path = shutil.which('luci-auth')
  if not luci_auth_path:
    logging.error("'luci-auth' binary not found. Is depot_tools not on PATH?")
    return False
  cmd = [luci_auth_path, 'info', '-scopes-context']
  try:
    subprocess.run(cmd,
                   stdout=subprocess.PIPE,
                   stderr=subprocess.STDOUT,
                   text=True,
                   check=True)
  except subprocess.CalledProcessError as e:
    logging.error('luci-auth context auth unavailable:')
    logging.error(e.output.strip())
    logging.error(
        "Please run 'luci-auth login -scopes-context' to authenticate, "
        'preferring your @google.com account if you have one.')
    return False
  return True


def get_prompt_resp(rerun_props):
  """Prompts the user for how to continue based on recipe output

  Args:
    rerun_props: A list of namedtuples[str, dict] containing the prompt to show
        and the dict of properties to use if that prompt is selected.
  Returns:
    Dict of properties to use for the next recipe invocation. None or an empty
        dict of properties indicate the recipe should not be reinvoked.
  """
  options = '/'.join(f'({option.prompt[0]}){option.prompt[1:]}'
                     for option in rerun_props)
  prompt = (f'How do you wish to proceed? Please enter {options} to confirm: ')
  resp = input(prompt).strip().lower()

  for option in rerun_props:
    # An empty resp will default to the first option like a --force run
    if option.prompt.lower().startswith(resp):
      return option.properties
  return None


class LegacyRunner:
  """Interface for running the UTR recipe via the legacy `recipes.py run` mode.

  TODO(crbug.com/326904531): Sometime in Q2 2024, a more modernized option for
  running recipes locally should be made available. This file can/should be
  updated to support and utilize that new mode if/when it's available.
  """

  def __init__(self,
               recipes_py,
               builder_props,
               project,
               bucket,
               builder,
               tests,
               skip_compile,
               skip_test,
               skip_prompts,
               build_dir=None,
               additional_test_args=None,
               reuse_task=None,
               skip_coverage=False):
    """Constructor for LegacyRunner

    Args:
      recipes_py: pathlib.Path to the root of the recipe bundle
      builder_props: Dict containing the props for the builder to run as.
      project: Project name of the builder to run as.
      bucket: Bucket name of the builder to run as.
      builder: Builder name of the builder to run as.
      tests: List of tests to run.
      skip_compile: If True, the UTR will only run the tests.
      skip_test: If True, the UTR will only compile.
      skip_prompts: If True, skip Y/N prompts for warnings.
      skip_coverage: If True, skip code coverage instrumentation.
      build_dir: pathlib.Path to the build dir to build in. Will use the UTR's
          default otherwise if needed.
      additional_test_args: List of additional args to pass to the tests.
      reuse_task: String of a swarming task to reuse.
    """
    self._recipes_py = recipes_py
    self._skip_coverage = skip_coverage
    self._skip_prompts = skip_prompts
    self._console_printer = console.Console()
    assert self._recipes_py.exists()

    # It's probably safe to assume chromium implies chromium-swarm and chrome
    # implies chrome-swarming. If it's not, cr-buildbucket.cfg attaches the
    # swarming to each and every builder. So could use that instead.
    self._swarming_server = 'chrome-swarming'
    self._utr_recipe = 'chrome/universal_test_runner'
    # Put all results in "try" realms. "try" should be writable for most devs,
    # while other realms like "ci" likely aren't. "try" is generally where we
    # confine untested code, so it's the best fit for our results here.
    self._luci_realm = 'chrome:try'
    if project == 'chromium':
      self._swarming_server = 'chromium-swarm'
      self._luci_realm = 'chromium:try'
      self._utr_recipe = 'chromium/universal_test_runner'

    # Add UTR recipe props. Its schema is located at:
    # https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipes/chromium/universal_test_runner.proto
    input_props = builder_props.copy()
    input_props['checkout_path'] = str(_SRC_DIR)
    input_props['$recipe_engine/path'] = {'cache_dir': str(_SRC_DIR.parent)}
    input_props['test_names'] = tests
    input_props['$build/chromium_swarming'] = {'task_realm': self._luci_realm}
    if additional_test_args:
      input_props['additional_test_args'] = additional_test_args
    if build_dir:
      input_props['build_dir'] = str(build_dir.absolute())
    # The recipe will overwrite this property so we have to put it preserve it
    # elsewhere
    if 'recipe' in input_props:
      input_props['builder_recipe'] = input_props['recipe']

    mode = 'RUN_TYPE_COMPILE_AND_RUN'
    assert not (skip_compile and skip_test)
    if skip_compile:
      mode = 'RUN_TYPE_RUN'
    elif skip_test:
      mode = 'RUN_TYPE_COMPILE'
    input_props['run_type'] = mode

    if reuse_task:
      input_props['reuse_swarming_task'] = reuse_task

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
    # TODO(crbug.com/41492688): Ensure the chrome version for internal builders
    # when they are added.
    # Set reclient and siso to use untrusted even for imitating ci builders
    if not '$build/reclient' in input_props:
      input_props['$build/reclient'] = {}
    input_props['$build/reclient']['instance'] = self._get_reclient_instance()
    if not '$build/siso' in input_props:
      input_props['$build/siso'] = {}
    input_props['$build/siso']['project'] = self._get_siso_project()
    self._input_props = input_props

  def _merge_rerun_props(self, rerun_props_from_recipe):
    """Merges user's preferred rerun props with the recipe's.

    The user may explicitly opt-out of some behavior controlled via rerun props.
    Use this method to make sure the recipe doesn't overwrite their preference.
    """
    merged_rerun_props = rerun_props_from_recipe.copy()
    if self._skip_coverage:
      merged_rerun_props['bypass_branch_check'] = True
      merged_rerun_props['skip_instrumentation'] = True
    return merged_rerun_props

  def _get_cmd_output(self, cmd):
    p = subprocess.run(cmd,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT,
                       text=True,
                       check=False)
    if p.returncode == 0:
      return p.stdout.strip()
    return ''

  def _get_reclient_instance(self):
    cmd = [
        'python3',
        str(_RECLIENT_CLI),
        '--get-rbe-instance',
    ]
    return self._get_cmd_output(cmd) or _DEFAULT_RBE_PROJECT

  def _get_siso_project(self):
    cmd = [
        'python3',
        str(_SISO_CLI),
        '--get-siso-project',
    ]
    return self._get_cmd_output(cmd) or _DEFAULT_RBE_PROJECT

  def _run(self, adapter, rerun_props=None):
    """Internal implementation of invoking `recipes.py run`.

    Args:
      adapter: A output_adapter.Adapter for parsing recipe output.
      rerun_props: Dict containing additional props to pass to the recipe.
    Returns:
      Tuple of
        exit code of the `recipes.py` invocation,
        summary markdown of the `recipes.py` invocation,
        a dict of rerun_props the recipe should be re-invoked with
    """
    input_props = self._input_props.copy()
    input_props['rerun_options'] = self._merge_rerun_props(rerun_props or {})
    with tempfile.TemporaryDirectory() as tmp_dir:

      output_path = pathlib.Path(tmp_dir).joinpath('out.json')
      rerun_props_path = pathlib.Path(tmp_dir).joinpath('rerun_props.json')
      input_props['output_properties_file'] = str(rerun_props_path)
      cmd = [
          'luci-auth',
          'context',
          '--',
          'rdb',
          'stream',
          '-new',
          '-realm',
          self._luci_realm,
          '--',
          self._recipes_py,
          'run',
          '--output-result-json',
          output_path,
          '--properties-file',
          '-',  # '-' means read from stdin
          self._utr_recipe,
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
        while not proc.stdout.at_eof():
          try:
            line = await proc.stdout.readline()
            adapter.ProcessLine(line.decode('utf-8').strip(os.linesep))
          except ValueError:
            logging.exception('Failed to parse line from the recipe')
        await proc.wait()
        return proc.returncode

      returncode = asyncio.run(exec_recipe())

      # Try to pull out the summary markdown from the recipe run.
      failure_md = ''
      if not output_path.exists():
        logging.error('Recipe output json not found')
      else:
        try:
          with open(output_path) as f:
            output = json.load(f)
          failure_md = output.get('failure', {}).get('humanReason', '')
          # TODO(crbug.com/41492688): Also pull out info about gclient/GN arg
          # mismatches, surface those as a Y/N prompt to the user, and re-run
          # if Y.
        except json.decoder.JSONDecodeError:
          logging.exception('Recipe output is invalid json')

      # If this file exists, the recipe is signalling to us that there's an
      # issue, and that we need to re-run if we're sure we want to proceed.
      # The contents of the file are the properties we should re-run it with.
      rerun_props = []
      if rerun_props_path.exists():
        with open(rerun_props_path) as f:
          raw_json = json.load(f)
          for prompt in raw_json:
            rerun_props.append(
                RerunOption(prompt=prompt[0], properties=prompt[1]))

      return returncode, failure_md, rerun_props

  def run_recipe(self, filter_stdout=True):
    """Runs the UTR recipe with the settings defined on the CLI.

    Args:
      filter_stdout: If True, filters noisy log output from the recipe.
    Returns:
      Tuple of (exit code, error message) of the `recipes.py` invocation.
    """
    rerun_props = None
    if filter_stdout:
      adapter = output_adapter.LegacyOutputAdapter()
    else:
      adapter = output_adapter.PassthroughAdapter()
    # We might need to run the recipe a handful of times before we receive a
    # final result. Put a cap on the amount of re-runs though, just in case.
    for _ in range(10):
      exit_code, failure_md, rerun_prop_options = self._run(
          adapter, rerun_props)
      # For in-line code snippets in markdown, style them as python. This
      # seems the least weird-looking.
      pretty_md = markdown.Markdown(failure_md, inline_code_lexer='python')
      if not rerun_prop_options:
        logging.warning('')
        if exit_code:
          # Use the markdown printer from "rich" to better format the text in
          # a terminal.
          md = pretty_md if pretty_md else 'Unknown error'
          self._console_printer.print(md, style='red')
        else:
          logging.info('[green]Success![/]')
        return exit_code, None  # Assume the recipe's failure_md is sufficient
      logging.warning('')
      self._console_printer.print(pretty_md)
      logging.warning('')
      if not self._skip_prompts:
        rerun_props = get_prompt_resp(rerun_prop_options)
      else:
        logging.warning(
            '[yellow]Proceeding despite the recipe warning due to the presence '
            'of "--force".[/]')
        if len(rerun_prop_options) < 1 or len(rerun_prop_options[0]) < 2:
          return 1, 'Received bad run options from the recipe'
        # Properties of the first option is the default path
        rerun_props = rerun_prop_options[0].properties
      if not rerun_props:
        return exit_code, 'User-aborted due to warning'
    return 1, 'Exceeded too many recipe re-runs'

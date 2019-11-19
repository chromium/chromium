# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import os
import sys

from telemetry import benchmark
from telemetry.internal.browser import browser_finder
from telemetry.internal.util import path as path_module


sys.path.append(os.path.join(os.path.dirname(__file__), '..',
                             '..', 'variations'))
import fieldtrial_util  # pylint: disable=import-error


# This function returns a list of two-tuples designed to extend
# browser_options.profile_files_to_copy. On success, it will return two entries:
# 1. The actual indexed ruleset file, which will be placed in a destination
#    directory as specified by the version found in the prefs file.
# 2. A default prefs 'Local State' file, which contains information about the ad
#    tagging ruleset's version.
def GetAdTaggingProfileFiles(chrome_output_directory):
  """Gets ad tagging tuples for browser_options.profile_files_to_copy.

  This function looks for input files related to ad tagging, and returns tuples
  indicating where those files should be copied to in the resulting perf
  benchmark profile directory.

  Args:
      chrome_output_directory: path to the output directory for this benchmark
      (e.g. out/Default).

  Returns:
      A list of two-tuples designed to extend profile_files_to_copy in
      BrowserOptions. If no ad tagging related input files could be found,
      returns an empty list.
  """
  if chrome_output_directory is None:
    return []

  gen_path = os.path.join(chrome_output_directory, 'gen', 'components',
                          'subresource_filter', 'tools')
  ruleset_path = os.path.join(gen_path, 'GeneratedRulesetData')
  if not os.path.exists(ruleset_path):
    return []

  local_state_path = os.path.join(gen_path, 'default_local_state.json')
  if not os.path.exists(local_state_path):
    return []

  with open(local_state_path, 'r') as f:
    state_json = json.load(f)
    ruleset_version = state_json['subresource_filter']['ruleset_version']

    # The ruleset should reside in:
    # Subresource Filter/Indexed Rules/<iv>/<uv>/Ruleset Data
    # Where iv = indexed version and uv = unindexed version
    ruleset_format_str = '%d' % ruleset_version['format']
    ruleset_dest = os.path.join('Subresource Filter', 'Indexed Rules',
                                ruleset_format_str, ruleset_version['content'],
                                'Ruleset Data')

    return [(ruleset_path, ruleset_dest), (local_state_path, 'Local State')]


class PerfBenchmark(benchmark.Benchmark):
  """ Super class for all benchmarks in src/tools/perf/benchmarks directory.
  All the perf benchmarks must subclass from this one to to make sure that
  the field trial configs are activated for the browser during benchmark runs.
  For more info, see: https://goo.gl/4uvaVM
  """

  def SetExtraBrowserOptions(self, options):
    """To be overridden by perf benchmarks."""
    pass

  def CustomizeOptions(self, finder_options):
    # Subclass of PerfBenchmark should override  SetExtraBrowserOptions to add
    # more browser options rather than overriding CustomizeOptions.
    super(PerfBenchmark, self).CustomizeOptions(finder_options)

    browser_options = finder_options.browser_options

    # Enable taking screen shot on failed pages for all perf benchmarks.
    browser_options.take_screenshot_for_failed_page = True

    # The current field trial config is used for an older build in the case of
    # reference. This is a problem because we are then subjecting older builds
    # to newer configurations that may crash.  To work around this problem,
    # don't add the field trials to reference builds.
    #
    # The same logic applies to the ad filtering ruleset, which could be in a
    # binary format that an older build does not expect.
    if (browser_options.browser_type != 'reference' and
        'no-field-trials' not in browser_options.compatibility_mode):
      variations = self._GetVariationsBrowserArgs(finder_options)
      browser_options.AppendExtraBrowserArgs(variations)

      browser_options.profile_files_to_copy.extend(
          GetAdTaggingProfileFiles(
              self._GetOutDirectoryEstimate(finder_options)))

    # A non-sandboxed, 120-seconds-delayed gpu process is currently running in
    # the browser to collect gpu info. A command line switch is added here to
    # skip this gpu process for all perf tests to prevent any interference
    # with the test results.
    browser_options.AppendExtraBrowserArgs(
        '--disable-gpu-process-for-dx12-vulkan-info-collection')

    # TODO(crbug.com/881469): remove this once Webview supports
    # VizDisplayCompositor.
    if (browser_options.browser_type and
        'android-webview' in browser_options.browser_type):
      browser_options.AppendExtraBrowserArgs(
          '--disable-features=VizDisplayCompositor')

    self.SetExtraBrowserOptions(browser_options)

  @staticmethod
  def FixupTargetOS(target_os):
    if target_os == 'darwin':
      return 'mac'
    if target_os.startswith('win'):
      return 'windows'
    if target_os.startswith('linux'):
      return 'linux'
    if target_os == 'cros':
      return 'chromeos'
    return target_os

  def _GetVariationsBrowserArgs(self, finder_options):
    chrome_root = finder_options.chrome_root
    if chrome_root is None:
      chrome_root = path_module.GetChromiumSrcDir()

    variations_dir = os.path.join(chrome_root, 'testing', 'variations')
    possible_browser = browser_finder.FindBrowser(finder_options)
    if not possible_browser:
      return []

    return fieldtrial_util.GenerateArgs(
        os.path.join(variations_dir, 'fieldtrial_testing_config.json'),
        [self.FixupTargetOS(possible_browser.target_os)])

  @staticmethod
  def _GetPossibleBuildDirectories(chrome_src_dir, browser_type):
    possible_directories = path_module.GetBuildDirectories(chrome_src_dir)
    # Special case "android-chromium" and "any" and check all
    # possible out directories.
    if browser_type in ('android-chromium', 'any'):
      return possible_directories

    # For all other browser types, just consider directories which match.
    return (p for p in possible_directories
            if os.path.basename(p).lower() == browser_type)

  def _GetOutDirectoryEstimate(self, finder_options):
    """Gets an estimate of the output directory for this build.

    Note that as an estimate, this may be incorrect. Callers should be aware of
    this and ensure that in the case that this returns an existing but
    incorrect directory, nothing should critically break.

    """
    if finder_options.chromium_output_dir is not None:
      return finder_options.chromium_output_dir

    possible_directories = self._GetPossibleBuildDirectories(
        finder_options.chrome_root, finder_options.browser_options.browser_type)
    return next((p for p in possible_directories if os.path.exists(p)), None)

  @staticmethod
  def IsSvelte(possible_browser):
    """Returns whether a possible_browser is on a svelte Android build."""
    if possible_browser.target_os == 'android':
      return possible_browser.platform.IsSvelte()
    return False

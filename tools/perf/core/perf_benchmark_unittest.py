# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import json
import shutil
import tempfile
import unittest

from telemetry.core import util
from telemetry.internal.browser import browser_finder
from telemetry.testing import options_for_unittests
from telemetry import decorators

from core import perf_benchmark


class PerfBenchmarkTest(unittest.TestCase):
  def setUp(self):
    self._output_dir = tempfile.mkdtemp()
    self._chrome_root = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._output_dir, ignore_errors=True)
    shutil.rmtree(self._chrome_root, ignore_errors=True)

  def _PopulateGenFiles(self, output_dir=None):
    root = output_dir if output_dir is not None else self._output_dir
    gen_path = os.path.join(root, 'gen', 'components', 'subresource_filter',
                            'tools')
    os.makedirs(gen_path)

    # Just make an empty ruleset file.
    open(os.path.join(gen_path, 'GeneratedRulesetData'), 'w').close()

    placeholder_json = {
        'subresource_filter' : {
            'ruleset_version' : {
                'content': '1000',
                'format': 100,
                'checksum': 0
            }
        }
    }
    with open(os.path.join(gen_path, 'default_local_state.json'), 'w') as f:
      json.dump(placeholder_json, f)


  def _ExpectAdTaggingProfileFiles(self, browser_options, expect_present):
    files_to_copy = browser_options.profile_files_to_copy

    local_state_to_copy = [
        (s, d) for (s, d) in files_to_copy if d == 'Local State']
    ruleset_data_to_copy = [
        (s, d) for (s, d) in files_to_copy if d.endswith('Ruleset Data')]

    num_expected_matches = 1 if expect_present else 0
    self.assertEqual(num_expected_matches, len(local_state_to_copy))
    self.assertEqual(num_expected_matches, len(ruleset_data_to_copy))

  # Flaky: https://crbug.com/1342706
  @decorators.Disabled('android-nougat', 'android-oreo')
  def testVariationArgs(self):
    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.chrome_root = self._output_dir
    if not options.browser_type:
      options.browser_type = "any"
    possible_browser = browser_finder.FindBrowser(options)
    if possible_browser is None:
      return
    target_os = perf_benchmark.PerfBenchmark.FixupTargetOS(
        possible_browser.target_os)
    self.assertIsNotNone(target_os)

    testing_config = json.dumps({
      "OtherPlatformStudy": [{
          "platforms": ["fake_platform"],
          "experiments": [{
              "name": "OtherPlatformFeature",
              "enable_features": ["NonExistentFeature"]
          }]
      }],
      "TestStudy": [{
          "platforms": [target_os],
          "experiments": [{
              "name": "TestFeature",
              "params": { "param1" : "value1" },
              "enable_features": ["Feature1", "Feature2"],
              "disable_features": ["Feature3", "Feature4"]}]}]})
    variations_dir = os.path.join(self._output_dir, "testing", "variations")
    os.makedirs(variations_dir)

    fieldtrial_path = os.path.join(
        variations_dir, "fieldtrial_testing_config.json")
    with open(fieldtrial_path, "w") as f:
      f.write(testing_config)

    benchmark.CustomizeOptions(options)

    # For non-Android, we expect to just pass the "--enable-field-trial-config"
    # flag. For Android, due to binary size constraints, the flag cannot be
    # used. We instead expect generated browser args from the testing config
    # file. See the FIELDTRIAL_TESTING_ENABLED buildflag definition in
    # components/variations/service/BUILD.gn for more details.
    if not perf_benchmark.PerfBenchmark.IsAndroid(possible_browser):
      expected_args = ['--enable-field-trial-config']
    else:
      expected_args = [
          "--enable-features=Feature1<TestStudy,Feature2<TestStudy",
          "--disable-features=Feature3<TestStudy,Feature4<TestStudy",
          "--force-fieldtrials=TestStudy/TestFeature",
          "--force-fieldtrial-params=TestStudy.TestFeature:param1/value1"
      ]

    for arg in expected_args:
      self.assertIn(arg, options.browser_options.extra_browser_args)

    # Test 'reference' type, which has no variation params applied by default.
    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.chrome_root = self._output_dir
    options.browser_options.browser_type = 'reference'
    benchmark.CustomizeOptions(options)

    for arg in expected_args:
      self.assertNotIn(arg, options.browser_options.extra_browser_args)

    # Test compatibility mode, which has no variation params applied by default.
    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.chrome_root = self._output_dir
    options.browser_options.compatibility_mode = ['no-field-trials']
    benchmark.CustomizeOptions(options)

    for arg in expected_args:
      self.assertNotIn(arg, options.browser_options.extra_browser_args)

  def testNoAdTaggingRuleset(self):
    # This tests (badly) assumes that util.GetBuildDirectories() will always
    # return a list of multiple directories, with Debug ordered before Release.
    # This is not the case if CHROMIUM_OUTPUT_DIR is set or a build.ninja file
    # exists in the current working directory - in those cases, only a single
    # directory is returned. So, abort early if we only get back one directory.
    num_dirs = 0
    for _ in util.GetBuildDirectories(self._chrome_root):
      num_dirs += 1
    if num_dirs < 2:
      return

    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()

    # Set the chrome root to avoid using a ruleset from an existing "Release"
    # out dir.
    options.chrome_root = self._output_dir
    benchmark.CustomizeOptions(options)
    self._ExpectAdTaggingProfileFiles(options.browser_options, False)

  def testAdTaggingRulesetReference(self):
    self._PopulateGenFiles()

    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.browser_options.browser_type = 'reference'

    # Careful, do not parse the command line flag for 'chromium-output-dir', as
    # that sets the global os environment variable CHROMIUM_OUTPUT_DIR,
    # affecting other tests. See http://crbug.com/843994.
    options.chromium_output_dir = self._output_dir

    benchmark.CustomizeOptions(options)
    self._ExpectAdTaggingProfileFiles(options.browser_options, False)

  def testAdTaggingRuleset(self):
    self._PopulateGenFiles()

    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()

    # Careful, do not parse the command line flag for 'chromium-output-dir', as
    # that sets the global os environment variable CHROMIUM_OUTPUT_DIR,
    # affecting other tests. See http://crbug.com/843994.
    options.chromium_output_dir = self._output_dir

    benchmark.CustomizeOptions(options)
    self._ExpectAdTaggingProfileFiles(options.browser_options, True)

  def testAdTaggingRulesetNoExplicitOutDir(self):
    self._PopulateGenFiles(os.path.join(self._chrome_root, 'out', 'Release'))

    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.chrome_root = self._chrome_root
    options.browser_options.browser_type = "release"

    benchmark.CustomizeOptions(options)
    self._ExpectAdTaggingProfileFiles(options.browser_options, True)

  def testAdTaggingRulesetNoExplicitOutDirAndroidChromium(self):
    self._PopulateGenFiles(os.path.join(self._chrome_root, 'out', 'Default'))

    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.chrome_root = self._chrome_root

    # android-chromium is special cased to search for anything.
    options.browser_options.browser_type = "android-chromium"

    benchmark.CustomizeOptions(options)
    self._ExpectAdTaggingProfileFiles(options.browser_options, True)

  def testAdTaggingRulesetOutputDirNotFound(self):
    # Same as the above test but use Debug instead of Release. This should
    # cause the benchmark to fail to find the ruleset because we only check
    # directories matching the browser_type.
    self._PopulateGenFiles(os.path.join(self._chrome_root, 'out', 'Debug'))

    # This tests (badly) assumes that util.GetBuildDirectories() will always
    # return a list of multiple directories, with Debug ordered before Release.
    # This is not the case if CHROMIUM_OUTPUT_DIR is set or a build.ninja file
    # exists in the current working directory - in those cases, only a single
    # directory is returned. So, abort early if we only get back one directory.
    num_dirs = 0
    for _ in util.GetBuildDirectories(self._chrome_root):
      num_dirs += 1
    if num_dirs < 2:
      return

    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.chrome_root = self._chrome_root
    options.browser_options.browser_type = "release"

    benchmark.CustomizeOptions(options)
    self._ExpectAdTaggingProfileFiles(options.browser_options, False)

  def testAdTaggingRulesetInvalidJson(self):
    self._PopulateGenFiles()
    json_path = os.path.join(
        self._output_dir, 'gen', 'components', 'subresource_filter', 'tools',
        'default_local_state.json')
    self.assertTrue(os.path.exists(json_path))
    with open(json_path, 'w') as f:
      f.write('{some invalid : json, 19')

    benchmark = perf_benchmark.PerfBenchmark()
    options = options_for_unittests.GetCopy()
    options.chromium_output_dir = self._output_dir

    # Should fail due to invalid JSON.
    with self.assertRaises(ValueError):
      benchmark.CustomizeOptions(options)

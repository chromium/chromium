# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import logging
import os
import time

from telemetry.testing import tab_test_case
from telemetry import decorators

import py_utils


# Possible ways that gl::Crash() will show up in a stack trace.
CRASH_SIGNATURES = [
    'gl::Crash',
    'chrome!Crash',
    'GpuServiceImpl::Crash()',
]


class BrowserMinidumpTest(tab_test_case.TabTestCase):
  def assertContainsAtLeastOne(self, expected_values, checked_value):
    for expected in expected_values:
      if expected in checked_value:
        return
    raise AssertionError(
        'None of %s found in %s' % (expected_values, checked_value))

  @decorators.Isolated
  # Minidump symbolization doesn't work in ChromeOS local mode if the rootfs is
  # still read-only, so skip the test in that case.
  @decorators.Disabled(
      'chromeos-local',
      'win7'  # https://crbug.com/1084931
  )
  def testSymbolizeMinidump(self):
    # Wait for the browser to restart fully before crashing
    self._LoadPageThenWait('var sam = "car";', 'sam')
    self._browser.tabs.New().Navigate('chrome://gpucrash', timeout=5)
    crash_minidump_path = self._browser.GetRecentMinidumpPathWithTimeout()
    self.assertIsNotNone(crash_minidump_path)

    if crash_minidump_path is not None:
      logging.info('testSymbolizeMinidump: most recent path = '
          + crash_minidump_path)
    all_paths = self._browser.GetAllMinidumpPaths()
    if all_paths is not None:
      logging.info('testSymbolizeMinidump: all paths ' + ''.join(all_paths))
    all_unsymbolized_paths = self._browser.GetAllUnsymbolizedMinidumpPaths()
    if all_unsymbolized_paths is not None:
      logging.info('testSymbolizeMinidump: all unsymbolized paths '
          + ''.join(all_unsymbolized_paths))

    # Flakes on chromeos: crbug.com/1014754
    # This has failed to repro either locally or on swarming, so dump extra
    # information if this is hit on the bots.
    if len(all_unsymbolized_paths) != 1:
      self._browser.CollectDebugData(logging.ERROR)
    self.assertTrue(len(all_unsymbolized_paths) == 1)

    # Now symbolize that minidump and make sure there are no longer any present
    succeeded, stack = self._browser.SymbolizeMinidump(crash_minidump_path)
    self.assertTrue(succeeded)
    self.assertContainsAtLeastOne(CRASH_SIGNATURES, stack)

    all_unsymbolized_after_symbolize_paths = \
        self._browser.GetAllUnsymbolizedMinidumpPaths()
    if all_unsymbolized_after_symbolize_paths is not None:
      logging.info('testSymbolizeMinidump: after symbolize all '
          + 'unsymbolized paths: '
          + ''.join(all_unsymbolized_after_symbolize_paths))
    self.assertTrue(len(all_unsymbolized_after_symbolize_paths) == 0)

  @decorators.Isolated
  # Minidump symbolization doesn't work in ChromeOS local mode if the rootfs is
  # still read-only, so skip the test in that case.
  @decorators.Disabled(
      'chromeos-local',
      'win7'  # https://crbug.com/1084931
  )
  def testMultipleCrashMinidumps(self):
    # Wait for the browser to restart fully before crashing
    self._LoadPageThenWait('var cat = "dog";', 'cat')
    self._browser.tabs.New().Navigate('chrome://gpucrash', timeout=5)
    first_crash_path = self._browser.GetRecentMinidumpPathWithTimeout()

    self.assertIsNotNone(first_crash_path)
    if first_crash_path is not None:
      logging.info('testMultipleCrashMinidumps: first crash most recent path'
          + first_crash_path)
    all_paths = self._browser.GetAllMinidumpPaths()
    if all_paths is not None:
      logging.info('testMultipleCrashMinidumps: first crash all paths: '
          + ''.join(all_paths))
    # Flakes on chromeos: crbug.com/1014754
    # This has failed to repro either locally or on swarming, so dump extra
    # information if this is hit on the bots.
    if len(all_paths) != 1:
      self._browser.CollectDebugData(logging.ERROR)
    self.assertEquals(len(all_paths), 1)
    self.assertEqual(all_paths[0], first_crash_path)
    all_unsymbolized_paths = self._browser.GetAllUnsymbolizedMinidumpPaths()
    self.assertTrue(len(all_unsymbolized_paths) == 1)
    if all_unsymbolized_paths is not None:
      logging.info('testMultipleCrashMinidumps: first crash all unsymbolized '
          'paths: ' + ''.join(all_unsymbolized_paths))

    # Restart the browser and then crash a second time
    self._RestartBrowser()

    # Start a new tab in the restarted browser
    self._LoadPageThenWait('var foo = "bar";', 'foo')

    self._browser.tabs.New().Navigate('chrome://gpucrash', timeout=5)
    # Make the oldest allowable timestamp slightly after the first dump's
    # timestamp so we don't get the first one returned to us again
    oldest_ts = os.path.getmtime(first_crash_path) + 1
    second_crash_path = self._browser.GetRecentMinidumpPathWithTimeout(
        oldest_ts=oldest_ts)
    self.assertIsNotNone(second_crash_path)
    if second_crash_path is not None:
      logging.info('testMultipleCrashMinidumps: second crash most recent path'
          + second_crash_path)
    second_crash_all_paths = self._browser.GetAllMinidumpPaths()
    if second_crash_all_paths is not None:
      logging.info('testMultipleCrashMinidumps: second crash all paths: '
          + ''.join(second_crash_all_paths))
    second_crash_all_unsymbolized_paths = \
        self._browser.GetAllUnsymbolizedMinidumpPaths()
    #self.assertTrue(len(all_unsymbolized_paths) == 1)
    if second_crash_all_unsymbolized_paths is not None:
      logging.info('testMultipleCrashMinidumps: second crash all unsymbolized '
          'paths: ' + ''.join(second_crash_all_unsymbolized_paths))
    self.assertEquals(len(second_crash_all_paths), 2)
    # Check that both paths are now present and unsymbolized
    self.assertTrue(first_crash_path in second_crash_all_paths)
    self.assertTrue(second_crash_path in second_crash_all_paths)
    self.assertTrue(len(second_crash_all_unsymbolized_paths) == 2)


    # Now symbolize one of those paths and assert that there is still one
    # unsymbolized
    succeeded, stack = self._browser.SymbolizeMinidump(second_crash_path)
    self.assertTrue(succeeded)
    self.assertContainsAtLeastOne(CRASH_SIGNATURES, stack)

    after_symbolize_all_paths = self._browser.GetAllMinidumpPaths()
    if after_symbolize_all_paths is not None:
      logging.info('testMultipleCrashMinidumps: after symbolize all paths: '
          + ''.join(after_symbolize_all_paths))
    self.assertEquals(len(after_symbolize_all_paths), 2)
    after_symbolize_all_unsymbolized_paths = \
        self._browser.GetAllUnsymbolizedMinidumpPaths()
    if after_symbolize_all_unsymbolized_paths is not None:
      logging.info('testMultipleCrashMinidumps: after symbolize all '
          + 'unsymbolized paths: '
          + ''.join(after_symbolize_all_unsymbolized_paths))
    self.assertEquals(after_symbolize_all_unsymbolized_paths,
        [first_crash_path])

    # Explicitly ignore the remaining minidump so that it isn't detected during
    # teardown by the test runner.
    self._browser.IgnoreMinidump(first_crash_path)

  def _LoadPageThenWait(self, script, value):
    # We are occasionally seeing these tests fail on the first load and
    # call to GetMostRecentMinidumpPath, where the directory is coming up empty.
    # We are hypothesizing, that although the browser is technically loaded,
    # some of chromes optimizations could still be running in the background
    # that potentially initializing the crash directory that we set with the
    # environment vairable BREAKPAD_DUMP_LOCATION in desktop_browser_backend.
    # Therefore, we are adding a 5 second wait for now to see if this can help
    # the problem until we determine if there is another chrome process we can
    # wait on to ensure that all browser load processes have finished
    time.sleep(5)
    new_tab = self._browser.tabs.New()
    new_tab.Navigate(self.UrlOfUnittestFile('blank.html'),
        script_to_evaluate_on_commit=script)
    # Wait until the javascript has run ensuring that
    # the new browser has restarted before we crash it again
    py_utils.WaitFor(lambda: new_tab.EvaluateJavaScript(value), 60)

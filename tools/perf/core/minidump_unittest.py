# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import logging
import os
import sys
import time

from telemetry.core import exceptions
from telemetry.internal.results import artifact_compatibility_wrapper as acw
from telemetry.internal.results import artifact_logger
from telemetry.testing import tab_test_case
from telemetry import decorators

import py_utils


# Possible ways that gl::Crash() will show up in a stack trace.
GPU_CRASH_SIGNATURES = [
    'gl::Crash',
    'chrome!Crash',
    'GpuServiceImpl::Crash()',
]
# Possible ways that a renderer process crash intentionally caused by DevTools
# can show up in a stack trace.
FORCED_RENDERER_CRASH_SIGNATURES = [
    'base::debug::BreakDebugger',
    'blink::DevToolsSession::IOSession::DispatchProtocolCommand',
    'blink::HandleChromeDebugURL',
    'chrome!DispatchProtocolCommand',
    'logging::LogMessage::~LogMessage',
]


def ContainsAtLeastOne(expected_values, checked_value):
  for expected in expected_values:
    if expected in checked_value:
      return True
  return False


class BrowserMinidumpTest(tab_test_case.TabTestCase):
  def setUp(self):
    # If something is wrong with minidump symbolization, we want to get all the
    # debugging information we can from the bots since it may be difficult to
    # reproduce the issue locally. So, use the full logger implementation.
    artifact_logger.RegisterArtifactImplementation(
        acw.FullLoggingArtifactImpl())
    super(BrowserMinidumpTest, self).setUp()

  def tearDown(self):
    super(BrowserMinidumpTest, self).tearDown()
    artifact_logger.RegisterArtifactImplementation(None)

  def assertContainsAtLeastOne(self, expected_values, checked_value):
    self.assertTrue(ContainsAtLeastOne(expected_values, checked_value),
                    'None of %s found in %s' % (expected_values, checked_value))

  @decorators.Isolated
  # Minidump symbolization doesn't work in ChromeOS local mode if the rootfs is
  # still read-only, so skip the test in that case.
  @decorators.Disabled(
      'chromeos-local',
      'win7',  # https://crbug.com/1084931
  )
  def testSymbolizeMinidump(self):
    # Wait for the browser to restart fully before crashing
    self._LoadPageThenWait('var sam = "car";', 'sam')
    self._browser.tabs.New().Navigate('chrome://gpucrash', timeout=10)
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
    self.assertContainsAtLeastOne(GPU_CRASH_SIGNATURES, stack)

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
      'win7',  # https://crbug.com/1084931
  )
  def testMultipleCrashMinidumps(self):
    # Wait for the browser to restart fully before crashing
    self._LoadPageThenWait('var cat = "dog";', 'cat')
    self._browser.tabs.New().Navigate('chrome://gpucrash', timeout=10)
    first_crash_path = self._browser.GetRecentMinidumpPathWithTimeout()

    self.assertIsNotNone(first_crash_path)
    if first_crash_path is not None:
      logging.info('testMultipleCrashMinidumps: first crash most recent path ' +
                   first_crash_path)
    all_paths = self._browser.GetAllMinidumpPaths()
    if all_paths is not None:
      logging.info('testMultipleCrashMinidumps: first crash all paths: '
          + ''.join(all_paths))
    # Flakes on chromeos: crbug.com/1014754
    # This has failed to repro either locally or on swarming, so dump extra
    # information if this is hit on the bots.
    if len(all_paths) != 1:
      self._browser.CollectDebugData(logging.ERROR)
    self.assertEqual(len(all_paths), 1)
    self.assertEqual(all_paths[0], first_crash_path)
    all_unsymbolized_paths = self._browser.GetAllUnsymbolizedMinidumpPaths()
    self.assertTrue(len(all_unsymbolized_paths) == 1)
    if all_unsymbolized_paths is not None:
      logging.info('testMultipleCrashMinidumps: first crash all unsymbolized '
          'paths: ' + ''.join(all_unsymbolized_paths))

    # Restart the browser and then crash a second time
    logging.info('Restarting the browser')
    self._RestartBrowser()

    # Start a new tab in the restarted browser
    self._LoadPageThenWait('var foo = "bar";', 'foo')

    self._browser.tabs.New().Navigate('chrome://gpucrash', timeout=10)
    # Make the oldest allowable timestamp slightly after the first dump's
    # timestamp so we don't get the first one returned to us again
    oldest_ts = os.path.getmtime(first_crash_path) + 1
    second_crash_path = self._browser.GetRecentMinidumpPathWithTimeout(
        oldest_ts=oldest_ts)
    self.assertIsNotNone(second_crash_path)
    if second_crash_path is not None:
      logging.info(
          'testMultipleCrashMinidumps: second crash most recent path ' +
          second_crash_path)
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
    self.assertEqual(len(second_crash_all_paths), 2)
    # Check that both paths are now present and unsymbolized
    self.assertTrue(first_crash_path in second_crash_all_paths)
    self.assertTrue(second_crash_path in second_crash_all_paths)
    self.assertTrue(len(second_crash_all_unsymbolized_paths) == 2)


    # Now symbolize one of those paths and assert that there is still one
    # unsymbolized
    succeeded, stack = self._browser.SymbolizeMinidump(second_crash_path)
    self.assertTrue(succeeded)
    self.assertContainsAtLeastOne(GPU_CRASH_SIGNATURES, stack)

    after_symbolize_all_paths = self._browser.GetAllMinidumpPaths()
    if after_symbolize_all_paths is not None:
      logging.info('testMultipleCrashMinidumps: after symbolize all paths: '
          + ''.join(after_symbolize_all_paths))
    self.assertEqual(len(after_symbolize_all_paths), 2)
    after_symbolize_all_unsymbolized_paths = \
        self._browser.GetAllUnsymbolizedMinidumpPaths()
    if after_symbolize_all_unsymbolized_paths is not None:
      logging.info('testMultipleCrashMinidumps: after symbolize all '
          + 'unsymbolized paths: '
          + ''.join(after_symbolize_all_unsymbolized_paths))
    self.assertEqual(after_symbolize_all_unsymbolized_paths, [first_crash_path])

    # Explicitly ignore the remaining minidump so that it isn't detected during
    # teardown by the test runner.
    self._browser.IgnoreMinidump(first_crash_path)

  @decorators.Isolated
  # Minidump symbolization doesn't work in ChromeOS local mode if the rootfs is
  # still read-only, so skip the test in that case.
  @decorators.Disabled(
      'chromeos-board-eve',  # b/312565719
      'chromeos-local',
      'win7',  # https://crbug.com/1084931
  )
  def testMinidumpFromRendererHang(self):
    """Tests that renderer hangs result in minidumps.

    Telemetry has logic for detecting renderer hangs and killing the renderer
    and GPU processes in such cases so we can get minidumps for diagnosing the
    root cause.
    """
    self._LoadPageThenWait('var cat = "dog";', 'cat')
    try:
      self._browser.tabs[-1].Navigate('chrome://hang', timeout=10)
    except exceptions.Error:
      # We expect the navigate to time out due to the hang.
      pass
    found_minidumps = False
    try:
      # Hung renderers are detected by JavaScript evaluation timing out, so
      # try to evaluate something to trigger that.
      # The timeout provided is the same one used for crashing the processes, so
      # don't make it too short.
      self._browser.tabs[-1].EvaluateJavaScript('var cat = "dog";', timeout=10)
    except exceptions.TimeoutException:
      # If we time out while crashing the renderer process, the minidump should
      # still exist, we just have to manually look for it instead of it being
      # part of the exception.
      all_paths = self._browser.GetAllMinidumpPaths()
      # We can't assert that we have exactly two minidumps because we can also
      # get one from the renderer process being notified of the GPU process
      # crash.
      num_paths = len(all_paths)
      self.assertTrue(num_paths in (2, 3),
                      'Got %d minidumps, expected 2 or 3' % num_paths)
      found_renderer = False
      found_gpu = False
      for p in all_paths:
        succeeded, stack = self._browser.SymbolizeMinidump(p)
        self.assertTrue(succeeded)
        try:
          self.assertContainsAtLeastOne(FORCED_RENDERER_CRASH_SIGNATURES, stack)
          # We don't assert that we haven't found a renderer crash yet since
          # we can potentially get multiple under normal circumstances.
          found_renderer = True
        except AssertionError:
          self.assertContainsAtLeastOne(GPU_CRASH_SIGNATURES, stack)
          self.assertFalse(found_gpu, 'Found two GPU crashes')
          found_gpu = True
      self.assertTrue(found_renderer and found_gpu)
      found_minidumps = True
    except exceptions.AppCrashException as e:
      self.assertTrue(e.is_valid_dump)
      # We should get one minidump from the GPU process (gl::Crash()) and one
      # minidump from the renderer process (base::debug::BreakDebugger()).
      self.assertContainsAtLeastOne(FORCED_RENDERER_CRASH_SIGNATURES,
                                    '\n'.join(e.stack_trace))
      # There appears to be a bug on older versions of Windows 10 where the GPU
      # minidump won't be found by the AppCrashException no matter how long we
      # wait after the crash takes place. So, look for it afterwards.
      if not ContainsAtLeastOne(GPU_CRASH_SIGNATURES, '\n'.join(e.stack_trace)):
        self.assertEqual(sys.platform, 'win32')
        minidumps = self._browser.GetAllUnsymbolizedMinidumpPaths()
        self.assertEqual(len(minidumps), 1)
        succeeded, stack = self._browser.SymbolizeMinidump(minidumps[0])
        self.assertTrue(succeeded)
        self.assertContainsAtLeastOne(GPU_CRASH_SIGNATURES, stack)
      found_minidumps = True
    self.assertTrue(found_minidumps)

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

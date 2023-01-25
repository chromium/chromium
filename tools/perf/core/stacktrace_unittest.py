# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import tempfile
import os

from telemetry.core import exceptions
from telemetry import decorators
from telemetry.testing import tab_test_case


class TabStackTraceTest(tab_test_case.TabTestCase):

  # Stack traces do not currently work on Mac 10.6.
  @decorators.Isolated
  @decorators.Disabled('snowleopard', 'chromeos-local')
  def testValidDump(self):
    with self.assertRaises(exceptions.DevtoolsTargetCrashException) as c:
      self._tab.Navigate('chrome://crash', timeout=10)
    self.assertTrue(c.exception.is_valid_dump)

  @decorators.Isolated
  @decorators.Disabled('chromeos-local')
  def testCrashSymbols(self):
    with self.assertRaises(exceptions.DevtoolsTargetCrashException) as c:
      self._tab.Navigate('chrome://crash', timeout=10)
    self.assertIn('CrashIntentionally', '\n'.join(c.exception.stack_trace))

  # Some platforms do not support full stack traces, this test requires only
  # minimal symbols to be available.
  @decorators.Isolated
  @decorators.Disabled('chromeos-local')
  def testCrashMinimalSymbols(self):
    with self.assertRaises(exceptions.DevtoolsTargetCrashException) as c:
      self._tab.Navigate('chrome://crash', timeout=10)
    self.assertIn('HandleRendererDebugURL',
                  '\n'.join(c.exception.stack_trace))

  # The breakpad file specific test only apply to platforms which use the
  # breakpad symbol format. This also must be tested in isolation because it can
  # potentially interfere with other tests symbol parsing.
  @decorators.Isolated
  @decorators.Enabled('linux')
  def testBadBreakpadFileIgnored(self):
    # pylint: disable=protected-access
    executable_path = self._browser._browser_backend._executable
    executable = os.path.basename(executable_path)
    with tempfile.NamedTemporaryFile(mode='wt',
                                     dir=os.path.dirname(executable_path),
                                     prefix=executable + '.breakpad',
                                     delete=True) as f:
      garbage_hash = 'ABCDEF1234567'
      f.write('MODULE PLATFORM ARCH %s %s' % (garbage_hash, executable))
      f.flush()
      with self.assertRaises(exceptions.DevtoolsTargetCrashException) as c:
        self._tab.Navigate('chrome://crash', timeout=10)

      # Stack trace should still work.
      self.assertIn('CrashIntentionally', '\n'.join(c.exception.stack_trace))

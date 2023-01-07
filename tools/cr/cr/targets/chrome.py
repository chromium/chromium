# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for the chrome targets."""

import cr


class ChromeTarget(cr.NamedTarget):
  NAME = 'chrome'
  CONFIG = cr.Config.From(
      CR_RUN_ARGUMENTS=cr.Config.Optional('-d "{CR_URL!e}"'),
      CR_TARGET_NAME='Chrome',
  )


class ChromeTestTarget(cr.NamedTarget):
  NAME = 'chrome_test'
  CONFIG = cr.Config.From(
      CR_TARGET_NAME='ChromeTest',
      CR_TEST_TYPE=cr.Target.INSTRUMENTATION_TEST,
      CR_RUN_DEPENDENCIES=[ChromeTarget.NAME],
  )

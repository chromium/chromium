# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for the chrome_public targets."""

import cr


class ChromePublicTarget(cr.NamedTarget):
  NAME = 'chrome_public'
  CONFIG = cr.Config.From(
      CR_RUN_ARGUMENTS=cr.Config.Optional('-d "{CR_URL!e}"'),
      CR_TARGET_NAME='ChromePublic',
      CR_PACKAGE='org.chromium.chrome',
      CR_ACTIVITY='com.google.android.apps.chrome.Main',
  )


class ChromePublicTestTarget(cr.NamedTarget):
  NAME = 'chrome_public_test'
  CONFIG = cr.Config.From(
      CR_TARGET_NAME='ChromePublicTest',
      CR_TEST_TYPE=cr.Target.INSTRUMENTATION_TEST,
      CR_RUN_DEPENDENCIES=[ChromePublicTarget.NAME],
  )

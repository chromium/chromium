# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The android specific platform implementation module."""

from __future__ import print_function

import os
import subprocess

import cr

class AndroidPlatform(cr.Platform):
  """The implementation of Platform for the android target."""

  ACTIVE = cr.Config.From(
      CR_ADB=os.path.join('{CR_SRC}', 'third_party', 'android_sdk', 'public',
          'platform-tools', 'adb'),
      CR_TARGET_SUFFIX='_apk',
      CR_BINARY=os.path.join('{CR_BUILD_DIR}', 'apks', '{CR_TARGET_NAME}.apk'),
      CR_ACTION='android.intent.action.VIEW',
      CR_PACKAGE='com.google.android.apps.{CR_TARGET}',
      CR_PROCESS='{CR_PACKAGE}',
      CR_ACTIVITY='.Main',
      CR_INTENT='{CR_PACKAGE}/{CR_ACTIVITY}',
      CR_TEST_RUNNER=os.path.join(
          '{CR_SRC}', 'build', 'android', 'test_runner.py'),
      CR_ADB_GDB=os.path.join('{CR_SRC}', 'build', 'android', 'adb_gdb'),
      CR_DEFAULT_TARGET='chrome_public',
      GN_ARG_target_os='"android"'
  )

  def __init__(self):
    super(AndroidPlatform, self).__init__()
    self._env = cr.Config('android-env', literal=True, export=True)
    self.detected_config.AddChild(self._env)

  @property
  def priority(self):
    return super(AndroidPlatform, self).priority + 1

  @property
  def paths(self):
    return []


class AndroidInitHook(cr.InitHook):
  """Android output directory init hook.

  This makes sure that your client is android capable when you try
  to make and android output directory.
  """

  @property
  def enabled(self):
    return cr.AndroidPlatform.GetInstance().is_active

  def Run(self, old_version, config):
    _ = old_version, config  # unused
    # Check we are an android capable client
    target_os = cr.context.gclient.get('target_os', [])
    if 'android' in target_os:
      return
    url = cr.context.gclient.get('solutions', [{}])[0].get('url')
    if (url.startswith('https://chrome-internal.googlesource.com/') and
        url.endswith('/internal/apps.git')):
      return
    print('This client is not android capable.')
    print('It can be made capable by adding android to the target_os list')
    print('in the .gclient file, and then syncing again.')
    if not cr.Host.YesNo('Would you like to upgrade this client?'):
      print('Abandoning the creation of and android output directory.')
      exit(1)
    target_os.append('android')
    cr.context.gclient['target_os'] = target_os
    cr.base.client.WriteGClient()
    print('Client updated.')
    print('You may need to sync before an output directory can be made.')
    if cr.Host.YesNo('Would you like to sync this client now?'):
      cr.SyncCommand.Sync(["--nohooks"])

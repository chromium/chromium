# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import loading
from telemetry import benchmark

# pylint: disable=protected-access
@benchmark.Info(emails=['cbiesinger@chromium.org'],
                documentation_url='https://bit.ly/loading-benchmarks')
class LoadingDesktopLayoutNg(loading.LoadingDesktop):
  """A benchmark that runs loading.desktop with the layoutng flag."""

  def SetExtraBrowserOptions(self, options):
    super(LoadingDesktopLayoutNg, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-blink-features=LayoutNG')

  @classmethod
  def Name(cls):
    return 'loading.desktop_layout_ng'


# pylint: disable=protected-access
@benchmark.Info(emails=['cbiesinger@chromium.org'],
                documentation_url='https://bit.ly/loading-benchmarks')
class LoadingMobileLayoutNg(loading.LoadingDesktop):
  """A benchmark that runs loading.mobile with the layoutng flag."""

  def SetExtraBrowserOptions(self, options):
    super(LoadingMobileLayoutNg, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-blink-features=LayoutNG')

  @classmethod
  def Name(cls):
    return 'loading.mobile_layout_ng'

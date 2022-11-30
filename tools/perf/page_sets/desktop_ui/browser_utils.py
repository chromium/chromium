# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time


def Resize(browser,
           tab_id,
           start_width=None,
           end_width=None,
           start_height=None,
           end_height=None,
           yoyo=False,
           repeat=1,
           steps=100,
           step_interval=0.01):
  window_id = browser.GetWindowForTarget(tab_id)['result']['windowId']
  for _ in range(repeat):
    if yoyo:
      ratios = list(range(steps + 1)) + list(range(steps, -1, -1))
    else:
      ratios = range(steps + 1)
    for i in ratios:
      ratio = 1.0 * i / steps
      bounds = {}
      if start_width is not None and end_width is not None:
        bounds['width'] = int(start_width * (1 - ratio) + end_width * ratio)
      if start_height is not None and end_height is not None:
        bounds['height'] = int(start_height * (1 - ratio) + end_height * ratio)
      browser.SetWindowBounds(window_id, bounds)
      if step_interval > 0:
        time.sleep(step_interval)

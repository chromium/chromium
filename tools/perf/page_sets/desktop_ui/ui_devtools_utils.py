# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from telemetry.internal.browser.ui_devtools import \
    MOUSE_EVENT_TYPE_MOUSE_PRESSED, \
    MOUSE_EVENT_TYPE_MOUSE_RELEASED, \
    MOUSE_EVENT_BUTTON_LEFT


def ClickOn(ui_devtools,
            class_name,
            index=0,
            x=0,
            y=0,
            button=MOUSE_EVENT_BUTTON_LEFT,
            click_interval=0.2):
  # "" means exact search for UI Devtools.
  node_id = ui_devtools.QueryNodes('"%s"' % class_name)[index]
  ui_devtools.DispatchMouseEvent(node_id, MOUSE_EVENT_TYPE_MOUSE_PRESSED, x, y,
                                 button)
  time.sleep(click_interval)
  ui_devtools.DispatchMouseEvent(node_id, MOUSE_EVENT_TYPE_MOUSE_RELEASED, x, y,
                                 button)
  time.sleep(click_interval)

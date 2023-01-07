# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
This file provide some helper functions to send mouse and
keyboard events to interact with the browser. Please see existing
usage for how to use the functions.

To simulate mouse events, first locate the UI element which has
the event listener attached to, then use the DispatchMouseEvent
API to send the desiring mouse events to it.

To simulate keyboard events. either locate the UI element which
has the event listener attached to or the top level window element
that contains it, then use the DispatchKeyEvent API to send the
desiring key events to it. Due to the current status of UI Devtools,
locating the top level window element is not supported on Mac OS yet.

Note that chrome has 2 classes of keyboard events: key stroke events and
character events(See definition from ui/events/event.h). In DispatchKeyEvent
API, key stroke events will propagate to the normal event flow where as
character events will bypass input method and send directly to the focused
text input client. Use PressKey to send key stroke events and InputText to
send character events.

The query language of UI Devtools is documented in docs/ui/ui_devtools/index.md,
Use search by tag by surrounding query with <>, which is useful to locate a
Window element. Use exact search by surrounding query with "", which is useful
by searching UI element classes with the exact name.
'''

import sys
import time

from telemetry.internal.browser.ui_devtools import \
    MOUSE_EVENT_TYPE_MOUSE_PRESSED, \
    MOUSE_EVENT_TYPE_MOUSE_RELEASED, \
    MOUSE_EVENT_BUTTON_LEFT, \
    KEY_EVENT_TYPE_KEY_PRESSED, \
    KEY_EVENT_TYPE_KEY_RELEASED
from telemetry.internal.actions.key_event import GetKey


def IsMac():
  return sys.platform == 'darwin'


# Modifier key definition from ui/events/event_constants.h
SHIFT_DOWN = 1 << 1
CONTROL_DOWN = 1 << 2
ALT_DOWN = 1 << 3
COMMAND_DOWN = 1 << 4
PLATFORM_ACCELERATOR = COMMAND_DOWN if IsMac() else CONTROL_DOWN


def ClickOn(ui_devtools,
            class_name=None,
            element_id=None,
            index=0,
            x=0,
            y=0,
            button=MOUSE_EVENT_BUTTON_LEFT,
            click_interval=0.2):
  '''
  Send mouse pressed and release events to an UI element.
  '''
  # "" means exact search for UI Devtools.
  if class_name is not None:
    node_id = ui_devtools.QueryNodes('"%s"' % class_name)[index]
  elif element_id is not None:
    node_id = ui_devtools.QueryNodes('id:%s' % element_id)[index]
  else:
    raise ValueError('Invalid class_name or element_id!')

  ui_devtools.DispatchMouseEvent(node_id, MOUSE_EVENT_TYPE_MOUSE_PRESSED, x, y,
                                 button)
  time.sleep(click_interval)
  ui_devtools.DispatchMouseEvent(node_id, MOUSE_EVENT_TYPE_MOUSE_RELEASED, x, y,
                                 button)
  time.sleep(click_interval)


def PressKey(ui_devtools, node_id, key_name, flags=0, key_interval=0.1):
  '''
  Send key stroke events to an UI element. Key names are defined in
  third_party/catapult/telemetry/telemetry/internal/actions/key_event.py
  '''
  key_code, _ = GetKey(key_name)
  ui_devtools.DispatchKeyEvent(node_id,
                               KEY_EVENT_TYPE_KEY_PRESSED,
                               key_code,
                               flags=flags)
  time.sleep(key_interval)
  ui_devtools.DispatchKeyEvent(node_id,
                               KEY_EVENT_TYPE_KEY_RELEASED,
                               key_code,
                               flags=flags)
  time.sleep(key_interval)


def InputText(ui_devtools, node_id, text, key_interval=0.1):
  '''
  Send text as character events to an UI element.
  '''
  for ch in text:
    ui_devtools.DispatchKeyEvent(node_id,
                                 KEY_EVENT_TYPE_KEY_PRESSED,
                                 key=ord(ch),
                                 is_char=True)
    time.sleep(key_interval)

#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SiteCompare module for simulating keyboard input.

This module contains functions that can be used to simulate a user
pressing keys on a keyboard. Support is provided for formatted strings
including special characters to represent modifier keys like CTRL and ALT
"""

import time             # for sleep
import win32api         # for keybd_event and VkKeyCode
import win32con         # Windows constants

# TODO(jhaas): Ask the readability people if this would be acceptable:
#
#  from win32con import VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, KEYEVENTF_KEYUP
#
# This is a violation of the style guide but having win32con. everywhere
# is just plain ugly, and win32con is a huge import for just a handful of
# constants


def PressKey(down, key):
  """Presses or unpresses a key.

  Uses keybd_event to simulate either depressing or releasing
  a key

  Args:
    down: Whether the key is to be pressed or released
    key:  Virtual key code of key to press or release
  """

  # keybd_event injects key events at a very low level (it's the
  # Windows API keyboard device drivers call) so this is a very
  # reliable way of simulating user input
  win32api.keybd_event(key, 0, (not down) * win32con.KEYEVENTF_KEYUP)


def TypeKey(key, keystroke_time=0):
  """Simulate a keypress of a virtual key.

  Args:
    key: which key to press
    keystroke_time: length of time (in seconds) to "hold down" the key
                    Note that zero works just fine

  Returns:
    None
  """

  # This just wraps a pair of PressKey calls with an intervening delay
  PressKey(True, key)
  time.sleep(keystroke_time)
  PressKey(False, key)


def TypeString(string_to_type,
               use_modifiers=False,
               keystroke_time=0,
               time_between_keystrokes=0):
  """Simulate typing a string on the keyboard.

  Args:
    string_to_type: the string to print
    use_modifiers: specifies whether the following modifier characters
      should be active:
      {abc}: type characters with ALT held down
      [abc]: type characters with CTRL held down
      \ escapes {}[] and treats these values as literal
      standard escape sequences are valid even if use_modifiers is false
      \p is "pause" for one second, useful when driving menus
      \1-\9 is F-key, \0 is F10

      TODO(jhaas): support for explicit control of SHIFT, support for
                   nonprintable keys (F-keys, ESC, arrow keys, etc),
                   support for explicit control of left vs. right ALT or SHIFT,
                   support for Windows key

    keystroke_time: length of time (in secondes) to "hold down" the key
    time_between_keystrokes: length of time (seconds) to pause between keys

  Returns:
    None
  """

  shift_held = win32api.GetAsyncKeyState(win32con.VK_SHIFT  ) < 0
  ctrl_held  = win32api.GetAsyncKeyState(win32con.VK_CONTROL) < 0
  alt_held   = win32api.GetAsyncKeyState(win32con.VK_MENU   ) < 0

  next_escaped = False
  escape_chars = {
    'a': '\a', 'b': '\b', 'f': '\f', 'n': '\n', 'r': '\r', 't': '\t', 'v': '\v'}

  for char in string_to_type:
    vk = None
    handled = False

    # Check to see if this is the start or end of a modified block (that is,
    # {abc} for ALT-modified keys or [abc] for CTRL-modified keys
    if use_modifiers and not next_escaped:
      handled = True
      if char == "{" and not alt_held:
        alt_held = True
        PressKey(True, win32con.VK_MENU)
      elif char == "}" and alt_held:
        alt_held = False
        PressKey(False, win32con.VK_MENU)
      elif char == "[" and not ctrl_held:
        ctrl_held = True
        PressKey(True, win32con.VK_CONTROL)
      elif char == "]" and ctrl_held:
        ctrl_held = False
        PressKey(False, win32con.VK_CONTROL)
      else:
        handled = False

    # If this is an explicitly-escaped character, replace it with the
    # appropriate code
    if next_escaped and char in escape_chars: char = escape_chars[char]

    # If this is \p, pause for one second.
    if next_escaped and char == 'p':
      time.sleep(1)
      next_escaped = False
      handled = True

    # If this is \(d), press F key
    if next_escaped and char.isdigit():
      fkey = int(char)
      if not fkey: fkey = 10
      next_escaped = False
      vk = win32con.VK_F1 + fkey - 1

    # If this is the backslash, the next character is escaped
    if not next_escaped and char == "\\":
      next_escaped = True
      handled = True

    # If we make it here, it's not a special character, or it's an
    # escaped special character which should be treated as a literal
    if not handled:
      next_escaped = False
      if not vk: vk = win32api.VkKeyScan(char)

      # VkKeyScan() returns the scan code in the low byte. The upper
      # byte specifies modifiers necessary to produce the given character
      # from the given scan code. The only one we're concerned with at the
      # moment is Shift. Determine the shift state and compare it to the
      # current state... if it differs, press or release the shift key.
      new_shift_held = bool(vk & (1<<8))

      if new_shift_held != shift_held:
        PressKey(new_shift_held, win32con.VK_SHIFT)
        shift_held = new_shift_held

      # Type the key with the specified length, then wait the specified delay
      TypeKey(vk & 0xFF, keystroke_time)
      time.sleep(time_between_keystrokes)

  # Release the modifier keys, if held
  if shift_held: PressKey(False, win32con.VK_SHIFT)
  if ctrl_held:  PressKey(False, win32con.VK_CONTROL)
  if alt_held:   PressKey(False, win32con.VK_MENU)


def main():
  # We're being invoked rather than imported. Let's do some tests

  # Press command-R to bring up the Run dialog
  PressKey(True, win32con.VK_LWIN)
  TypeKey(ord('R'))
  PressKey(False, win32con.VK_LWIN)

  # Wait a sec to make sure it comes up
  time.sleep(1)

  # Invoke Notepad through the Run dialog
  TypeString("wordpad\n")

  # Wait another sec, then start typing
  time.sleep(1)
  TypeString("This is a test of SiteCompare's Keyboard.py module.\n\n")
  TypeString("There should be a blank line above and below this one.\n\n")
  TypeString("This line has control characters to make "
             "[b]boldface text[b] and [i]italic text[i] and normal text.\n\n",
             use_modifiers=True)
  TypeString(r"This line should be typed with a visible delay between "
             "characters. When it ends, there should be a 3-second pause, "
             "then the menu will select File/Exit, then another 3-second "
             "pause, then No to exit without saving. Ready?\p\p\p{f}x\p\p\pn",
             use_modifiers=True,
             keystroke_time=0.05,
             time_between_keystrokes=0.05)


if __name__ == "__main__":
  sys.exit(main())

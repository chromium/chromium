#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SiteCompare module for simulating mouse input.

This module contains functions that can be used to simulate a user
navigating using a pointing device. This includes mouse movement,
clicking with any button, and dragging.
"""

import time                 # for sleep

import win32api             # for mouse_event
import win32con             # Windows constants
import win32gui             # for window functions


def ScreenToMouse(pt):
  """Convert a value in screen coordinates to mouse coordinates.

  Mouse coordinates are specified as a percentage of screen dimensions,
  normalized to 16 bits. 0 represents the far left/top of the screen,
  65535 represents the far right/bottom. This function assumes that
  the size of the screen is fixed at module load time and does not change

  Args:
    pt: the point of the coords to convert

  Returns:
    the converted point
  """

  # Initialize the screen dimensions on first execution. Note that this
  # function assumes that the screen dimensions do not change during run.
  if not ScreenToMouse._SCREEN_DIMENSIONS:
    desktop = win32gui.GetClientRect(win32gui.GetDesktopWindow())
    ScreenToMouse._SCREEN_DIMENSIONS = (desktop[2], desktop[3])

  return ((65535 * pt[0]) / ScreenToMouse._SCREEN_DIMENSIONS[0],
          (65535 * pt[1]) / ScreenToMouse._SCREEN_DIMENSIONS[1])

ScreenToMouse._SCREEN_DIMENSIONS = None


def PressButton(down, button='left'):
  """Simulate a mouse button press or release at the current mouse location.

  Args:
    down: whether the button is pressed or released
    button: which button is pressed

  Returns:
    None
  """

  # Put the mouse_event flags in a convenient dictionary by button
  flags = {
    'left':   (win32con.MOUSEEVENTF_LEFTUP,   win32con.MOUSEEVENTF_LEFTDOWN),
    'middle': (win32con.MOUSEEVENTF_MIDDLEUP, win32con.MOUSEEVENTF_MIDDLEDOWN),
    'right':  (win32con.MOUSEEVENTF_RIGHTUP,  win32con.MOUSEEVENTF_RIGHTDOWN)
    }

  # hit the button
  win32api.mouse_event(flags[button][down], 0, 0)


def ClickButton(button='left', click_time=0):
  """Press and release a mouse button at the current mouse location.

  Args:
    button: which button to click
    click_time: duration between press and release

  Returns:
    None
  """
  PressButton(True, button)
  time.sleep(click_time)
  PressButton(False, button)


def DoubleClickButton(button='left', click_time=0, time_between_clicks=0):
  """Double-click a mouse button at the current mouse location.

  Args:
    button: which button to click
    click_time: duration between press and release
    time_between_clicks: time to pause between clicks

  Returns:
    None
  """
  ClickButton(button, click_time)
  time.sleep(time_between_clicks)
  ClickButton(button, click_time)


def MoveToLocation(pos, duration=0, tick=0.01):
  """Move the mouse cursor to a specified location, taking the specified time.

  Args:
    pos: position (in screen coordinates) to move to
    duration: amount of time the move should take
    tick: amount of time between successive moves of the mouse

  Returns:
    None
  """
  # calculate the number of moves to reach the destination
  num_steps = (duration/tick)+1

  # get the current and final mouse position in mouse coords
  current_location = ScreenToMouse(win32gui.GetCursorPos())
  end_location = ScreenToMouse(pos)

  # Calculate the step size
  step_size = ((end_location[0]-current_location[0])/num_steps,
               (end_location[1]-current_location[1])/num_steps)
  step = 0

  while step < num_steps:
    # Move the mouse one step
    current_location = (current_location[0]+step_size[0],
                        current_location[1]+step_size[1])

    # Coerce the coords to int to avoid a warning from pywin32
    win32api.mouse_event(
      win32con.MOUSEEVENTF_MOVE|win32con.MOUSEEVENTF_ABSOLUTE,
      int(current_location[0]), int(current_location[1]))

    step += 1
    time.sleep(tick)


def ClickAtLocation(pos, button='left', click_time=0):
  """Simulate a mouse click in a particular location, in screen coordinates.

  Args:
    pos: position in screen coordinates (x,y)
    button: which button to click
    click_time: duration of the click

  Returns:
    None
  """
  MoveToLocation(pos)
  ClickButton(button, click_time)


def ClickInWindow(hwnd, offset=None, button='left', click_time=0):
  """Simulate a user mouse click in the center of a window.

  Args:
    hwnd: handle of the window to click in
    offset: where to click, defaults to dead center
    button: which button to click
    click_time: duration of the click

  Returns:
    Nothing
  """

  rect = win32gui.GetClientRect(hwnd)
  if offset is None: offset = (rect[2]/2, rect[3]/2)

  # get the screen coordinates of the window's center
  pos = win32gui.ClientToScreen(hwnd, offset)

  ClickAtLocation(pos, button, click_time)


def DoubleClickInWindow(
  hwnd, offset=None, button='left', click_time=0, time_between_clicks=0.1):
  """Simulate a user mouse double click in the center of a window.

  Args:
    hwnd: handle of the window to click in
    offset: where to click, defaults to dead center
    button: which button to click
    click_time: duration of the clicks
    time_between_clicks: length of time to pause between clicks

  Returns:
    Nothing
  """
  ClickInWindow(hwnd, offset, button, click_time)
  time.sleep(time_between_clicks)
  ClickInWindow(hwnd, offset, button, click_time)


def main():
  # We're being invoked rather than imported. Let's do some tests

  screen_size = win32gui.GetClientRect(win32gui.GetDesktopWindow())
  screen_size = (screen_size[2], screen_size[3])

  # move the mouse (instantly) to the upper right corner
  MoveToLocation((screen_size[0], 0))

  # move the mouse (over five seconds) to the lower left corner
  MoveToLocation((0, screen_size[1]), 5)

  # click the left mouse button. This will open up the Start menu
  # if the taskbar is at the bottom

  ClickButton()

  # wait a bit, then click the right button to open the context menu
  time.sleep(3)
  ClickButton('right')

  # move the mouse away and then click the left button to dismiss the
  # context menu
  MoveToLocation((screen_size[0]/2, screen_size[1]/2), 3)
  MoveToLocation((0, 0), 3)
  ClickButton()


if __name__ == "__main__":
  sys.exit(main())

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_WINDOW_EVENT_TARGET_H_
#define UI_BASE_WIN_WINDOW_EVENT_TARGET_H_

#include <windows.h>

#include "base/component_export.h"

namespace ui {

// This interface is implemented by classes who get input events forwarded to
// them from others. E.g. would be a win32 parent child relationship where the
// child forwards input events to the parent after doing minimal processing.
class COMPONENT_EXPORT(UI_BASE) WindowEventTarget {
 public:
  static const char kWin32InputEventTarget[];

  // Handles mouse events like WM_MOUSEMOVE, WM_LBUTTONDOWN, etc.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are dependent on the type of the
  // message.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleMouseMessage(unsigned int message,
                                     WPARAM w_param,
                                     LPARAM l_param,
                                     bool* handled) = 0;

  // Handles pointer events like WM_POINTERUP, WM_POINTERDOWN, WM_POINTERUPDATE
  // events.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are as per MSDN docs.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandlePointerMessage(unsigned int message,
                                       WPARAM w_param,
                                       LPARAM l_param,
                                       bool* handled) = 0;

  // Handles keyboard events like WM_KEYDOWN/WM_KEYUP, etc.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are dependent on the type of the
  // message.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleKeyboardMessage(unsigned int message,
                                        WPARAM w_param,
                                        LPARAM l_param,
                                        bool* handled) = 0;

  // Handles WM_TOUCH events.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are as per MSDN docs.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleTouchMessage(unsigned int message,
                                     WPARAM w_param,
                                     LPARAM l_param,
                                     bool* handled) = 0;

  // Handles WM_INPUT events.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are as per MSDN docs.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleInputMessage(unsigned int message,
                                     WPARAM w_param,
                                     LPARAM l_param,
                                     bool* handled) = 0;

  // Handles scroll messages like WM_VSCROLL and WM_HSCROLL.
  // The |message| parameter identifies the scroll message.
  // The |w_param| and |l_param| values are dependent on the type of scroll.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  virtual LRESULT HandleScrollMessage(unsigned int message,
                                      WPARAM w_param,
                                      LPARAM l_param,
                                      bool* handled) = 0;

  // Handles the WM_NCHITTEST message
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are as per MSDN docs.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleNcHitTestMessage(unsigned int message,
                                         WPARAM w_param,
                                         LPARAM l_param,
                                         bool* handled) = 0;

  // Notification from the forwarder window that its parent changed.
  virtual void HandleParentChanged() = 0;

  // Apply the transform from Direct Manipulation API.

  // Calls ApplyPinchZoomScale() for pinch-zoom gesture. scale is the scale
  // factor.
  virtual void ApplyPinchZoomScale(float scale) = 0;

  // Pinch gesture phase. The sequencing expected of these events.
  // The sequence of calls is ApplyPinchZoomBegin(), any number of calls to
  // ApplyPinchZoomScale() and finally ApplyPinchZoomEnd().
  virtual void ApplyPinchZoomBegin() = 0;
  virtual void ApplyPinchZoomEnd() = 0;

  // Calls ApplyPanGestureScroll() for pan gesture, scroll_x and scroll_y are
  // pixel precison scroll offset.
  virtual void ApplyPanGestureScroll(int scroll_x, int scroll_y) = 0;

  // Calls ApplyPanGestureFling() for pan inertia gesture, scroll_x and scroll_y
  // are pixel precison scroll offset.
  virtual void ApplyPanGestureFling(int scroll_x, int scroll_y) = 0;

  // Pan gesture phase. The sequencing expected of these events.
  // The sequence of calls is ApplyPanGestureScrollBegin(), any number of calls
  // to ApplyPanGestureScroll(), ApplyPanGestureScrollEnd(),
  // ApplyPanGestureFlingBegin(), any number of calls to ApplyPanGestureFling(),
  // and finally ApplyPanGestureFlingEnd().
  // |transition_to_pinch| is a hint to know if the scroll end will be followed
  // by a pinch begin or not, so that momentum_phase can be set to Blocked if
  // a momentum scroll/fling will not be happening next.
  virtual void ApplyPanGestureScrollBegin(int scroll_x, int scroll_y) = 0;
  virtual void ApplyPanGestureScrollEnd(bool transition_to_pinch) = 0;
  virtual void ApplyPanGestureFlingBegin() = 0;
  virtual void ApplyPanGestureFlingEnd() = 0;

 protected:
  WindowEventTarget();
  virtual ~WindowEventTarget();
};

}  // namespace ui

#endif  // UI_BASE_WIN_WINDOW_EVENT_TARGET_H_



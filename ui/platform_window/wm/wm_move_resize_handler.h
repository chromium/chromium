// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_WM_WM_MOVE_RESIZE_HANDLER_H_
#define UI_PLATFORM_WINDOW_WM_WM_MOVE_RESIZE_HANDLER_H_

#include "base/component_export.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {

class PlatformWindow;

class COMPONENT_EXPORT(WM) WmMoveResizeHandler {
 public:
  // A system window manager starts interactive drag or resize of a window based
  // on the |hittest| value. The |hittest| value identifies in which direction
  // the window should be resized or whether it should be moved. See
  // ui/base/hit_test.h for a concrete example with chromium symbolic names
  // defined. The |pointer_location_in_px| indicates the position of the button
  // press with respect to the platform window in screen pixel coordinates,
  // which is needed when sending a move/resize request in such backends as X11.
  // See _NET_WM_MOVERESIZE section in
  // https://specifications.freedesktop.org/wm-spec/1.4/ar01s04.html.
  //
  // There is no need to implement this by all the platforms except Ozone/X11
  // and Ozone/Wayland, compositors of which support interactive move/resize.
  //
  // This API must be used on mouse or touch events, which are targeted for
  // non-client components (check ui/base/hit_test.h again) except the ones
  // targeted for components like HTMAXBUTTON. In that case, the mouse events
  // are used to identify clicks on maximize/minimize/restore buttons located in
  // the top non-client area of the chromium window. See
  // WindowEventFilter::OnMouseEvent for a concrete example of how mouse events
  // are identified as client or non-client.
  //
  // When the API is called, there is no way to know that the call was
  // successful or not. The browser continues performing as usual except that a
  // system compositor does not send any mouse/keyboard/etc events until user
  // releases a mouse button. Instead, the compositor sends new bounds, which a
  // client uses to recreate gpu buffers and redraw visual represantation of the
  // browser.
  virtual void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) = 0;

 protected:
  virtual ~WmMoveResizeHandler() {}
};

COMPONENT_EXPORT(WM)
void SetWmMoveResizeHandler(PlatformWindow* platform_window,
                            WmMoveResizeHandler* move_resize_handler);
COMPONENT_EXPORT(WM)
WmMoveResizeHandler* GetWmMoveResizeHandler(
    const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_WM_WM_MOVE_RESIZE_HANDLER_H_

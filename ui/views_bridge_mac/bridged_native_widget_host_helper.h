// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BRIDGE_MAC_BRIDGED_NATIVE_WIDGET_HOST_HELPER_H_
#define UI_VIEWS_BRIDGE_MAC_BRIDGED_NATIVE_WIDGET_HOST_HELPER_H_

#include "ui/base/ui_base_types.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/decorated_text.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views_bridge_mac/views_bridge_mac_export.h"

@class NSView;

namespace views_bridge_mac {

class DragDropClient;

// This is a helper class for the mojo interface BridgedNativeWidgetHost.
// This provides an easier-to-use interface than the mojo for selected
// functions. It also is temporarily exposing functionality that is not yet
// implemented over mojo.
class VIEWS_BRIDGE_MAC_EXPORT BridgedNativeWidgetHostHelper {
 public:
  virtual ~BridgedNativeWidgetHostHelper() = default;

  // Retrieve the NSView for accessibility for this widget.
  // TODO(ccameron): This interface cannot be implemented over IPC. A scheme
  // for implementing accessibility across processes needs to be designed and
  // implemented.
  virtual NSView* GetNativeViewAccessible() = 0;

  // Synchronously dispatch a key event. Note that this function will modify
  // |event| based on whether or not it was handled.
  virtual void DispatchKeyEvent(ui::KeyEvent* event) = 0;

  // Synchronously dispatch a key event to the current menu controller (if one
  // exists and it is owned by the widget for this). Return true if the event
  // was swallowed (that is, if the menu's dispatch returned
  // POST_DISPATCH_NONE). Note that this function will modify |event| based on
  // whether or not it was handled.
  virtual bool DispatchKeyEventToMenuController(ui::KeyEvent* event) = 0;

  // Synchronously query the quicklook text at |location_in_content|. Return in
  // |found_word| whether or not a word was found.
  // TODO(ccameron): This needs gfx::DecoratedText to be mojo-ified before it
  // can be done over mojo.
  virtual void GetWordAt(const gfx::Point& location_in_content,
                         bool* found_word,
                         gfx::DecoratedText* decorated_word,
                         gfx::Point* baseline_point) = 0;

  // Returns the vertical position that sheets should be anchored, in pixels
  // from the bottom of the window.
  // TODO(ccameron): This should be either moved to the mojo interface or
  // separated out in such a way as to avoid needing to go through mojo.
  virtual double SheetPositionY() = 0;

  // Return a pointer to host's DragDropClientMac.
  // TODO(ccameron): Drag-drop behavior needs to be implemented over mojo.
  virtual DragDropClient* GetDragDropClient() = 0;
};

}  // namespace views_bridge_mac

#endif  // UI_VIEWS_BRIDGE_MAC_BRIDGED_NATIVE_WIDGET_HOST_HELPER_H_

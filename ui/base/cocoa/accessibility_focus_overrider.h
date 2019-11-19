// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_ACCESSIBILITY_FOCUS_OVERRIDER_H_
#define UI_BASE_COCOA_ACCESSIBILITY_FOCUS_OVERRIDER_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// This object is used to enable cross-process accessibility focus querying.
//
// The following bullet points describe the need for this class:
// * The NSViews in out-of-process NSViews (e.g, for PWAs) will return a
//   NSRemoteAccessibilityElement from -[NSView accessibilityFocusedUIElement].
// * The focus query is then continued in the browser process by a call to
//   -[NSApplication accessibilityFocusedUIElement].
// * The default implementation -[NSApplication accessibilityFocusedUIElement]
//   in turn queries -[NSApplication keyWindow]'s accessibilityFocusedUIElement
//   method.
// * This is not what the PWA process wants. The PWA process wants its focus,
//   not the browser's focus.
// * To make this happen, when the PWA process' NSViews are focused, they
//   force the browser's  -[NSApplication accessibilityFocusedUIElement] to
//   return their accessibility focus.
//
// The above-required overriding of focus is done by instantiating an
// AccessibilityFocusOverrider and updating its state when the NSView in the
// PWA process is focused.
class UI_BASE_EXPORT AccessibilityFocusOverrider {
 public:
  class Client {
   public:
    virtual id GetAccessibilityFocusedUIElement() = 0;
  };

  AccessibilityFocusOverrider(Client* client);
  ~AccessibilityFocusOverrider();

  // Indicate if the NSApplication that is viewing this element is not this
  // process. Focus overriding is only needed for cross-process focus.
  void SetAppIsRemote(bool app_is_remote);

  // Indicate whether or not the view's window is currently key. This object
  // will override the application's focused accessibility element only if its
  // window is key (and the view is the window's first responder).
  void SetWindowIsKey(bool window_is_key);

  // Indicate whether or not the view is its window's first responder. This
  // object will override the application's focused accessibility element only
  // if the view is the window's first responder (and its window is key).
  void SetViewIsFirstResponder(bool view_is_first_responder);

  // Return the overridden focus, or nil if there is no overridden focus.
  static id GetFocusedUIElement();

 private:
  void UpdateOverriddenKeyElement();
  bool app_is_remote_ = false;
  bool window_is_key_ = false;
  bool view_is_first_responder_ = false;
  Client* const client_;
};

}  // namespace ui

#endif  // UI_BASE_COCOA_ACCESSIBILITY_FOCUS_OVERRIDER_H_

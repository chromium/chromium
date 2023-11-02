// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/accessibility_focus_overrider.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

namespace ui {

namespace {
AccessibilityFocusOverrider::Client* g_overridden_focused_element = nil;
}  // namespace

AccessibilityFocusOverrider::AccessibilityFocusOverrider(Client* client)
    : client_(client) {}

AccessibilityFocusOverrider::~AccessibilityFocusOverrider() {
  if (g_overridden_focused_element == client_)
    g_overridden_focused_element = nullptr;
}

void AccessibilityFocusOverrider::SetAppIsRemote(bool app_is_remote) {
  app_is_remote_ = app_is_remote;
  UpdateOverriddenKeyElement();
}

void AccessibilityFocusOverrider::SetWindowIsKey(bool window_is_key) {
  window_is_key_ = window_is_key;
  UpdateOverriddenKeyElement();
}

void AccessibilityFocusOverrider::SetViewIsFirstResponder(
    bool view_is_first_responder) {
  view_is_first_responder_ = view_is_first_responder;
  UpdateOverriddenKeyElement();
}

void AccessibilityFocusOverrider::UpdateOverriddenKeyElement() {
  if (app_is_remote_ && window_is_key_ && view_is_first_responder_) {
    g_overridden_focused_element = client_;
  } else if (g_overridden_focused_element == client_) {
    g_overridden_focused_element = nullptr;
  }
}

// static
id AccessibilityFocusOverrider::GetFocusedUIElement() {
  if (g_overridden_focused_element)
    return g_overridden_focused_element->GetAccessibilityFocusedUIElement();
  return nil;
}

}  // namespace ui

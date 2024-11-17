// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/widget_focus_manager.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace views {

// WidgetFocusManager ----------------------------------------------------------

// static
WidgetFocusManager* WidgetFocusManager::GetInstance() {
  static base::NoDestructor<WidgetFocusManager> instance;
  return instance.get();
}

WidgetFocusManager::~WidgetFocusManager() = default;

void WidgetFocusManager::AddFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  focus_change_listeners_.AddObserver(listener);
}

void WidgetFocusManager::RemoveFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  focus_change_listeners_.RemoveObserver(listener);
}

void WidgetFocusManager::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (enabled_) {
    focus_change_listeners_.Notify(
        &WidgetFocusChangeListener::OnNativeFocusChanged, focused_now);
  }
}

WidgetFocusManager::WidgetFocusManager() = default;

// AutoNativeNotificationDisabler ----------------------------------------------

AutoNativeNotificationDisabler::AutoNativeNotificationDisabler() {
  WidgetFocusManager::GetInstance()->DisableNotifications();
}

AutoNativeNotificationDisabler::~AutoNativeNotificationDisabler() {
  WidgetFocusManager::GetInstance()->EnableNotifications();
}

}  // namespace views

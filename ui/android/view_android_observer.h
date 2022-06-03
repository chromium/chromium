// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_OBSERVER_H_
#define UI_ANDROID_VIEW_ANDROID_OBSERVER_H_

#include "ui/android/ui_android_export.h"

namespace ui {

class UI_ANDROID_EXPORT ViewAndroidObserver {
 public:
  // Notifies that view gets attached to window. Note that the notification
  // is not sent if view is already in attached state.
  virtual void OnAttachedToWindow() = 0;

  // Notifies that view gets detached from window. Note that the notification
  // is not sent if view is already in detached state.
  virtual void OnDetachedFromWindow() = 0;

  // Notifies the view has been destroyed.
  virtual void OnViewAndroidDestroyed() {}

 protected:
  virtual ~ViewAndroidObserver() {}
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_OBSERVER_H_

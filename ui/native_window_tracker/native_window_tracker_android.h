// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_ANDROID_H_
#define UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_ANDROID_H_

#include "base/component_export.h"
#include "base/scoped_observation.h"
#include "ui/android/view_android_observer.h"
#include "ui/native_window_tracker/native_window_tracker.h"

namespace ui {

class COMPONENT_EXPORT(UI_NATIVE_WINDOW_TRACKER) NativeWindowTrackerAndroid
    : public NativeWindowTracker,
      public ViewAndroidObserver {
 public:
  explicit NativeWindowTrackerAndroid(WindowAndroid* window);

  NativeWindowTrackerAndroid(const NativeWindowTrackerAndroid&) = delete;
  NativeWindowTrackerAndroid& operator=(const NativeWindowTrackerAndroid&) =
      delete;

  ~NativeWindowTrackerAndroid() override;

  // NativeWindowTracker:
  bool WasNativeWindowDestroyed() const override;

 private:
  // ViewAndroidObserver:
  void OnViewAndroidDestroyed() override;

  base::ScopedObservation<ViewAndroid, ViewAndroidObserver> window_observation_{
      this};
};

}  // namespace ui

#endif  // UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_ANDROID_H_

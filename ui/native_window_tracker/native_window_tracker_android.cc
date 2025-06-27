// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_window_tracker/native_window_tracker_android.h"

#include <memory>

#include "ui/android/window_android.h"

namespace ui {

NativeWindowTrackerAndroid::NativeWindowTrackerAndroid(WindowAndroid* window) {
  window_observation_.Observe(window);
}

NativeWindowTrackerAndroid::~NativeWindowTrackerAndroid() = default;

bool NativeWindowTrackerAndroid::WasNativeWindowDestroyed() const {
  return !window_observation_.IsObserving();
}

void NativeWindowTrackerAndroid::OnViewAndroidDestroyed() {
  window_observation_.Reset();
}

// static
std::unique_ptr<NativeWindowTracker> NativeWindowTracker::Create(
    gfx::NativeWindow window) {
  return std::make_unique<NativeWindowTrackerAndroid>(window);
}

}  // namespace ui

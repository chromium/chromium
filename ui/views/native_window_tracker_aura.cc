// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/native_window_tracker_aura.h"

#include <memory>

#include "ui/aura/window.h"

namespace views {

NativeWindowTrackerAura::NativeWindowTrackerAura(gfx::NativeWindow window)
    : window_(window) {
  window->AddObserver(this);
}

NativeWindowTrackerAura::~NativeWindowTrackerAura() {
  if (window_)
    window_->RemoveObserver(this);
}

bool NativeWindowTrackerAura::WasNativeWindowDestroyed() const {
  return window_ == nullptr;
}

void NativeWindowTrackerAura::OnWindowDestroying(aura::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

// static
std::unique_ptr<NativeWindowTracker> NativeWindowTracker::Create(
    gfx::NativeWindow window) {
  return std::make_unique<NativeWindowTrackerAura>(window);
}

}  // namespace views

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_manager.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "ui/ozone/platform/x11/x11_window.h"

namespace ui {

namespace {

X11WindowManager* g_instance = nullptr;

}  // namespace

X11WindowManager::X11WindowManager() {
  DCHECK(!g_instance) << "There should only be a single X11WindowManager";
  g_instance = this;
}

X11WindowManager::~X11WindowManager() = default;

// static
X11WindowManager* X11WindowManager::GetInstance() {
  if (!g_instance) {
    auto manager = std::make_unique<X11WindowManager>();
    X11WindowManager* manager_ptr = manager.release();
    DCHECK_EQ(g_instance, manager_ptr);
  }
  return g_instance;
}

void X11WindowManager::GrabEvents(X11Window* window) {
  DCHECK_NE(located_events_grabber_, window);

  // Grabbing the mouse is asynchronous. However, we synchronously start
  // forwarding all mouse events received by Chrome to the
  // aura::WindowEventDispatcher which has capture. This makes capture
  // synchronous for all intents and purposes if either:
  // - |located_events_grabber_| is set to have capture.
  // OR
  // - The topmost window underneath the mouse is managed by Chrome.
  auto* old_grabber = located_events_grabber_.get();

  // Update |located_events_grabber_| prior to calling OnXWindowLostCapture() to
  // avoid releasing pointer grab.
  located_events_grabber_ = window;
  if (old_grabber)
    old_grabber->OnXWindowLostCapture();

  // the X11Window calls GrabPointer by itself.
}

void X11WindowManager::UngrabEvents(X11Window* window) {
  DCHECK_EQ(located_events_grabber_, window);
  // Release mouse grab asynchronously. A window managed by Chrome is likely
  // the topmost window underneath the mouse so the capture release being
  // asynchronous is likely inconsequential.
  auto* old_grabber = located_events_grabber_.get();
  located_events_grabber_ = nullptr;
  old_grabber->OnXWindowLostCapture();
}

void X11WindowManager::AddWindow(X11Window* window) {
  DCHECK(window);
  auto widget = window->GetWidget();
  DCHECK_NE(gfx::kNullAcceleratedWidget, widget);
  DCHECK(!base::Contains(windows_, widget));
  windows_.emplace(widget, window);
}

void X11WindowManager::RemoveWindow(X11Window* window) {
  DCHECK(window);
  auto widget = window->GetWidget();
  auto it = windows_.find(widget);
  // The XWindow might not have been initialized due to some errors.
  if (widget == gfx::kNullAcceleratedWidget) {
    DCHECK(it == windows_.end());
  } else {
    CHECK(it != windows_.end(), base::NotFatalUntil::M130);
    if (window_mouse_currently_on_ == it->second)
      window_mouse_currently_on_ = nullptr;
    windows_.erase(it);
  }
}

X11Window* X11WindowManager::GetWindow(gfx::AcceleratedWidget widget) const {
  auto it = windows_.find(widget);
  return it != windows_.end() ? it->second : nullptr;
}

void X11WindowManager::MouseOnWindow(X11Window* window) {
  if (window_mouse_currently_on_ == window)
    return;

  window_mouse_currently_on_ = window;
  window->OnMouseEnter();
}

}  // namespace ui

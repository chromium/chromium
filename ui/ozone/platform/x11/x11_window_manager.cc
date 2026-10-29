// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_manager.h"

#include <algorithm>

#include "ui/base/x/x11_pointer_grab.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
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
  DCHECK(!windows_.contains(widget));
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
    CHECK(it != windows_.end());
    if (window_mouse_currently_on_ == it->second)
      window_mouse_currently_on_ = nullptr;
    windows_.erase(it);
  }
  base::EraseIf(active_touches_, [window](const auto& pair) {
    return pair.second.window == window;
  });
}

X11Window* X11WindowManager::GetWindow(gfx::AcceleratedWidget widget) const {
  auto it = windows_.find(widget);
  return it != windows_.end() ? it->second : nullptr;
}

bool X11WindowManager::IsTrackingPointer() const {
  return ui::HasActivePointerGrab() ||
         std::ranges::any_of(windows_, [](const auto& pair) {
           return pair.second->has_pointer();
         });
}

bool X11WindowManager::HasWindowPendingMap() const {
  return std::ranges::any_of(
      windows_, [](const auto& pair) { return pair.second->IsMapPending(); });
}

void X11WindowManager::MouseOnWindow(X11Window* window) {
  if (window_mouse_currently_on_ == window)
    return;

  window_mouse_currently_on_ = window;
  window->OnCursorUpdate();
}

void X11WindowManager::OnTouchEventDispatched(X11Window* window,
                                              const TouchEvent& event) {
  const PointerId id = event.pointer_details().id;
  switch (event.type()) {
    case EventType::kTouchPressed:
      // A press for an id that is still tracked supersedes the earlier
      // sequence, whose end was never seen.
      active_touches_[id] = {.id = id,
                             .window = window,
                             .location = event.location_f(),
                             .root_location = event.root_location_f()};
      break;
    case EventType::kTouchMoved:
      if (auto it = active_touches_.find(id);
          it != active_touches_.end() && it->second.window == window) {
        it->second.location = event.location_f();
        it->second.root_location = event.root_location_f();
      }
      break;
    case EventType::kTouchReleased:
    case EventType::kTouchCancelled:
      active_touches_.erase(id);
      break;
    default:
      break;
  }
}

std::vector<X11WindowManager::ActiveTouch> X11WindowManager::TakeActiveTouches(
    const std::vector<PointerId>& ids) {
  std::vector<ActiveTouch> touches;
  for (PointerId id : ids) {
    if (auto it = active_touches_.find(id); it != active_touches_.end()) {
      touches.push_back(it->second);
      active_touches_.erase(it);
    }
  }
  return touches;
}

std::vector<X11WindowManager::ActiveTouch>
X11WindowManager::TakeAllActiveTouches() {
  std::vector<ActiveTouch> touches;
  for (const auto& [id, touch] : active_touches_) {
    touches.push_back(touch);
  }
  active_touches_.clear();
  return touches;
}

}  // namespace ui

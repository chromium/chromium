// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/wm_sync.h"

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace x11 {

namespace {

Window GetWindowForEvent(const Event& event) {
  if (auto* property = event.As<PropertyNotifyEvent>()) {
    return property->window;
  }
  if (auto* reparent = event.As<ReparentNotifyEvent>()) {
    return reparent->window;
  }
  if (auto* configure = event.As<ConfigureNotifyEvent>()) {
    return configure->window;
  }
  return Window::None;
}

}  // namespace

WmSync::WmSync(Connection* connection, base::OnceClosure on_synced)
    : WmSync(connection, std::move(on_synced), connection->synced_with_wm()) {}

WmSync::WmSync(Connection* connection,
               base::OnceClosure on_synced,
               bool sync_with_wm)
    : connection_(connection), on_synced_(std::move(on_synced)) {
  if (!sync_with_wm) {
    connection_->GetInputFocus().OnResponse(base::BindOnce(
        &WmSync::OnGetInputFocusResponse, weak_ptr_factory_.GetWeakPtr()));
    connection_->Flush();
    return;
  }

  constexpr EventMask event_mask =
      EventMask::StructureNotify | EventMask::PropertyChange;

  scoped_observation_.Observe(connection_);

  window_ = connection_->GenerateId<Window>();
  connection_->CreateWindow({
      .wid = window_,
      .parent = connection_->default_root(),
      // The window is never mapped, so the position doesn't matter.  However,
      // it's important that the following ConfigureWindow request has different
      // coordinates.
      .x = -10000,
      .y = -10000,
      .width = 10,
      .height = 10,
      .event_mask = event_mask,
  });
  window_events_ = connection_->ScopedSelectEvent(window_, event_mask);

  // Send a ConfigureWindow request. If a WM is running, the request will be
  // routed to the WM for it to service (or if a WM is not running, the X11
  // server will handle it).  Some time later, we'll observe the ConfigureNotify
  // event that signals the sync is complete.
  connection_->ConfigureWindow({
      .window = window_,
      .x = -200,
      .y = -200,
      .width = 20,
      .height = 20,
  });
  connection_->Flush();
}

WmSync::~WmSync() {
  Cleanup();
}

void WmSync::OnEvent(const Event& xevent) {
  if (window_ != Window::None && GetWindowForEvent(xevent) == window_) {
    Cleanup();
    std::move(on_synced_).Run();
    // `this` may be deleted.
  }
}

void WmSync::OnGetInputFocusResponse(GetInputFocusResponse response) {
  Cleanup();
  std::move(on_synced_).Run();
  // `this` may be deleted.
}

void WmSync::Cleanup() {
  if (window_ == Window::None) {
    return;
  }

  scoped_observation_.Reset();
  window_events_.Reset();
  connection_->DestroyWindow(window_);
  window_ = Window::None;
}

}  // namespace x11

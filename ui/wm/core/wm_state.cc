// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/wm_state.h"

#include "ui/events/platform/platform_event_source.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/transient_window_controller.h"
#include "ui/wm/core/transient_window_stacking_client.h"

namespace wm {

WMState::WMState()
    : window_stacking_client_(new TransientWindowStackingClient),
      transient_window_client_(new TransientWindowController),
      capture_controller_(std::make_unique<CaptureController>()) {
  aura::client::SetWindowStackingClient(window_stacking_client_.get());
  aura::client::SetTransientWindowClient(transient_window_client_.get());
}

WMState::~WMState() {
  if (aura::client::GetWindowStackingClient() == window_stacking_client_.get())
    aura::client::SetWindowStackingClient(NULL);

  if (aura::client::GetTransientWindowClient() ==
      transient_window_client_.get()) {
    aura::client::SetTransientWindowClient(NULL);
  }
}

}  // namespace wm

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"

namespace ui {

AXEventRecorder::AXEventRecorder() = default;
AXEventRecorder::~AXEventRecorder() = default;

void AXEventRecorder::ListenToEvents(AXEventCallback callback) {
  AXEventCallback old_callback;
  {
    base::AutoLock lock(on_event_lock_);
    old_callback = std::exchange(callback_, std::move(callback));
  }
}

void AXEventRecorder::StopListeningToEvents() {
  ListenToEvents(base::NullCallback());
}

void AXEventRecorder::OnEvent(const std::string& event) {
  AXEventCallback callback;
  {
    base::AutoLock lock(on_event_lock_);
    event_logs_.push_back(event);
    callback = callback_;
  }
  if (callback) {
    callback.Run(event);
  }
}

std::vector<std::string> AXEventRecorder::GetEventLogs() const {
  base::AutoLock lock(on_event_lock_);
  return event_logs_;
}

}  // namespace ui

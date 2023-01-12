// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"

namespace ui {

AXEventRecorder::AXEventRecorder() = default;
AXEventRecorder::~AXEventRecorder() = default;

void AXEventRecorder::StopListeningToEvents() {
  callback_ = base::NullCallback();
}

void AXEventRecorder::OnEvent(const std::string& event) {
  base::AutoLock lock{on_event_lock_};
  event_logs_.push_back(event);
  if (callback_)
    callback_.Run(event);
}

const std::vector<std::string> AXEventRecorder::GetEventLogs() const {
  base::AutoLock lock{on_event_lock_};
  return event_logs_;
}

}  // namespace ui

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder.h"

#include "base/callback_helpers.h"
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

bool AXEventRecorder::IsRunUntilEventSatisfied(
    const std::vector<std::string>& run_until) const {
  base::AutoLock lock{on_event_lock_};
  // If no @*-RUN-UNTIL-EVENT directives, then having any events is enough.
  LOG(ERROR) << "=== IsRunUntilEventSatisfied#1 run_until size="
             << run_until.size();
  if (run_until.empty())
    return true;

  LOG(ERROR) << "=== IsRunUntilEventSatisfied#2 Logs size="
             << event_logs_.size();

  for (const std::string& event : event_logs_)
    for (const std::string& query : run_until)
      if (event.find(query) != std::string::npos)
        return true;

  return false;
}

std::vector<std::string> AXEventRecorder::GetEventLogs() const {
  base::AutoLock lock{on_event_lock_};
  return event_logs_;
}

}  // namespace ui

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder.h"

#include "base/callback_helpers.h"

namespace ui {

AXEventRecorder::AXEventRecorder() = default;
AXEventRecorder::~AXEventRecorder() = default;

void AXEventRecorder::StopListeningToEvents() {
  callback_ = base::NullCallback();
}

void AXEventRecorder::OnEvent(const std::string& event) {
  event_logs_.push_back(event);
  if (callback_)
    callback_.Run(event);
}

}  // namespace ui

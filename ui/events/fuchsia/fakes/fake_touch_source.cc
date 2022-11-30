// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/fakes/fake_touch_source.h"

#include "base/notreached.h"

namespace ui {

FakeTouchSource::FakeTouchSource() = default;

FakeTouchSource::~FakeTouchSource() = default;

// |fuchsia.ui.pointer.TouchSource|
void FakeTouchSource::Watch(
    std::vector<fuchsia::ui::pointer::TouchResponse> responses,
    TouchSource::WatchCallback callback) {
  responses_ = std::move(responses);
  callback_ = std::move(callback);
}

// Have the server issue events to the client's hanging-get Watch call.
void FakeTouchSource::ScheduleCallback(
    std::vector<fuchsia::ui::pointer::TouchEvent> events) {
  CHECK(callback_) << "require a valid WatchCallback";
  callback_(std::move(events));
}

// Allow the test to observe what the client uploaded on the next Watch call.
absl::optional<std::vector<fuchsia::ui::pointer::TouchResponse>>
FakeTouchSource::UploadedResponses() {
  auto responses = std::move(responses_);
  responses_.reset();
  return responses;
}

void FakeTouchSource::UpdateResponse(
    fuchsia::ui::pointer::TouchInteractionId interaction_id,
    fuchsia::ui::pointer::TouchResponse response,
    TouchSource::UpdateResponseCallback callback) {
  NOTREACHED();
}

}  // namespace ui

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_FAKES_FAKE_TOUCH_SOURCE_H_
#define UI_EVENTS_FUCHSIA_FAKES_FAKE_TOUCH_SOURCE_H_

#include <fuchsia/ui/pointer/cpp/fidl.h>

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {

// A test stub to act as the protocol server. A test can control what is sent
// back by this server implementation, via the ScheduleCallback call.
class FakeTouchSource : public fuchsia::ui::pointer::TouchSource {
 public:
  FakeTouchSource();
  ~FakeTouchSource() override;

  // |fuchsia.ui.pointer.TouchSource|
  void Watch(std::vector<fuchsia::ui::pointer::TouchResponse> responses,
             TouchSource::WatchCallback callback) override;

  // Have the server issue events to the client's hanging-get Watch call.
  void ScheduleCallback(std::vector<fuchsia::ui::pointer::TouchEvent> events);

  // Allow the test to observe what the client uploaded on the next Watch call.
  absl::optional<std::vector<fuchsia::ui::pointer::TouchResponse>>
  UploadedResponses();

 private:
  // |fuchsia.ui.pointer.TouchSource|
  void UpdateResponse(fuchsia::ui::pointer::TouchInteractionId interaction_id,
                      fuchsia::ui::pointer::TouchResponse response,
                      TouchSource::UpdateResponseCallback callback) override;

  // Client uploads responses to server.
  absl::optional<std::vector<fuchsia::ui::pointer::TouchResponse>> responses_;

  // Client-side logic to invoke on Watch() call's return. A test triggers it
  // with ScheduleCallback().
  TouchSource::WatchCallback callback_;
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_FAKES_FAKE_TOUCH_SOURCE_H_

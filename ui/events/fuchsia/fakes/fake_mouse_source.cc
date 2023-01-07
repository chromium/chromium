// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/fakes/fake_mouse_source.h"

#include "base/check.h"

namespace ui {

FakeMouseSource::FakeMouseSource() = default;

FakeMouseSource::~FakeMouseSource() = default;

// |fuchsia.ui.pointer.MouseSource|
void FakeMouseSource::Watch(MouseSource::WatchCallback callback) {
  callback_ = std::move(callback);
}

// Have the server issue events to the client's hanging-get Watch call.
void FakeMouseSource::ScheduleCallback(
    std::vector<fuchsia::ui::pointer::MouseEvent> events) {
  CHECK(callback_) << "require a valid WatchCallback";
  callback_(std::move(events));
}

}  // namespace ui

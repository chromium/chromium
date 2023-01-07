// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/tests/fake_view_ref_focused.h"

#include "base/check.h"
#include "base/logging.h"

namespace ui {

FakeViewRefFocused::FakeViewRefFocused() = default;

FakeViewRefFocused::~FakeViewRefFocused() = default;

void FakeViewRefFocused::Watch(WatchCallback callback) {
  callback_ = std::move(callback);
  ++times_watched_;
}

void FakeViewRefFocused::ScheduleCallback(bool focused) {
  fuchsia::ui::views::FocusState focus_state;
  focus_state.set_focused(focused);
  CHECK(callback_);
  callback_(std::move(focus_state));
}

std::size_t FakeViewRefFocused::times_watched() const {
  return times_watched_;
}

}  // namespace ui

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/test/testing_cursor_client_observer.h"

namespace wm {

TestingCursorClientObserver::TestingCursorClientObserver() = default;

void TestingCursorClientObserver::reset() {
  cursor_visibility_ = did_visibility_change_ = false;
  custom_cursor_size_.reset();
  did_custom_cursor_size_change_ = false;
}

void TestingCursorClientObserver::OnCursorVisibilityChanged(bool is_visible) {
  cursor_visibility_ = is_visible;
  did_visibility_change_ = true;
}

void TestingCursorClientObserver::OnCustomCursorSizeChanged(
    std::optional<int> custom_cursor_size) {
  custom_cursor_size_ = custom_cursor_size;
  did_custom_cursor_size_change_ = true;
}

}  // namespace wm

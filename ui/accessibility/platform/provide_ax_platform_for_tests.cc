// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/provide_ax_platform_for_tests.h"

namespace ui {

ProvideAXPlatformForTests::ProvideAXPlatformForTests() {
  ax_platform_.emplace();
}

ProvideAXPlatformForTests::~ProvideAXPlatformForTests() {
  // exo::wayland tests call `RUN_ALL_TESTS()` from a different thread than the
  // one that the bulk of the work is done on.
  ax_platform_->DetachFromThread();
}

void ProvideAXPlatformForTests::OnTestEnd(
    const ::testing::TestInfo& test_info) {
  // exo::wayland tests call `RUN_ALL_TESTS()` from a different thread than the
  // one that the bulk of the work is done on.
  ax_platform_->DetachFromThread();
  ax_platform_.emplace();
}

}  // namespace ui

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/provide_ax_platform_for_tests.h"

namespace ui {

ProvideAXPlatformForTests::ProvideAXPlatformForTests() {
  ax_platform_.emplace();
}

ProvideAXPlatformForTests::~ProvideAXPlatformForTests() {
  ax_platform_.reset();
}

void ProvideAXPlatformForTests::OnTestEnd(
    const ::testing::TestInfo& test_info) {
  ax_platform_.reset();
  ax_platform_.emplace();
}

}  // namespace ui

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_for_test.h"

namespace ui {

AXMode AXPlatformForTest::GetProcessMode() {
  return mode_;
}

void AXPlatformForTest::SetProcessMode(AXMode new_mode) {
  mode_ = new_mode;
}

}  // namespace ui

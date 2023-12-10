// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_for_test.h"

#include <utility>

namespace ui {

AXMode AXPlatformForTest::GetProcessMode() {
  return mode_;
}

void AXPlatformForTest::SetProcessMode(AXMode new_mode) {
  const AXMode old_mode = std::exchange(mode_, new_mode);

  // Broadcast the new mode flags, if any, to the AXModeObservers.
  if (const auto additions = new_mode & ~old_mode; !additions.is_mode_off()) {
    ax_platform_.NotifyModeAdded(additions);
  }
}

}  // namespace ui

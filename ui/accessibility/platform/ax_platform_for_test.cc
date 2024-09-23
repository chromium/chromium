// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_for_test.h"

#include <utility>

#include "base/check_op.h"

namespace ui {

namespace {

AXPlatformForTest* g_instance = nullptr;

}  // namespace

// static
AXPlatformForTest& AXPlatformForTest::GetInstance() {
  CHECK_NE(g_instance, nullptr);
  return *g_instance;
}

AXPlatformForTest::AXPlatformForTest() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AXPlatformForTest::~AXPlatformForTest() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

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

void AXPlatformForTest::OnAccessibilityApiUsage() {}

#if BUILDFLAG(IS_WIN)
AXPlatform::ProductStrings AXPlatformForTest::GetProductStrings() {
  return {{}, {}, {}};
}
#endif

}  // namespace ui

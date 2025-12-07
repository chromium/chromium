// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_for_test.h"

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

void AXPlatformForTest::DetachFromThread() {
  ax_platform_.DetachFromThreadForTesting();
}

AXMode AXPlatformForTest::GetAccessibilityMode() {
  return mode_;
}

#if BUILDFLAG(IS_WIN)
AXPlatform::ProductStrings AXPlatformForTest::GetProductStrings() {
  return {{}, {}, {}};
}
#endif

}  // namespace ui

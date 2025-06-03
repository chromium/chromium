// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/provide_ax_platform_for_tests.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <tuple>

#include "ui/accessibility/platform/ax_platform_node_win.h"
#else
#include "ui/accessibility/platform/ax_platform_node_base.h"
#endif

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

  // Dear reader: If your test is failing here, it is because the test is either
  // leaking UX objects (e.g., Views, Widgets, etc) or is using accessibility
  // APIs that are leaking AXPlatformNode instances.
#if BUILDFLAG(IS_WIN)
  auto [instance_count, dormant_count, live_count, ghost_count] =
      ui::AXPlatformNodeWin::ResetCountsForTesting();
  EXPECT_EQ(ghost_count, 0U)
      << "This test is leaking COM interface references. If the test is not "
         "explicitly using facilities such as IAccessible, then you may have "
         "just found a bug in ui/accessibility/platform. Contact a member of "
         "ui/accessibility/OWNERS for guidance.";
#elif BUILDFLAG(IS_LINUX)
  // The `AuraLinuxApplication` singleton is never destroyed, so one node is
  // leaked in all unit tests that create a ViewAXPlatformNodeDelegate. It's not
  // possible to distinguish between a leak of an arbitrary node and a leak of
  // the AuraLinuxApplication's node, so there is no leak checking at all.
  // TODO(accessibility): Consider moving AuraLinuxApplication into AXPlatform
  // in some way so that it can be properly destroyed at shutdown.
  // TODO(accessibility): Investigate platform node leaks on Linux; see
  // https://crrev.com/c/chromium/src/+/6316732?checksPatchset=10&tab=checks.
  size_t instance_count = 0;
#else
  size_t instance_count =
      ui::AXPlatformNodeBase::ResetInstanceCountForTesting();
#endif
  EXPECT_EQ(instance_count, 0U)
      << "This test is leaking UX objects (e.g., Views or Widgets). This is "
         "often caused by ownership issues with Widgets and NativeWidgets";
}

}  // namespace ui

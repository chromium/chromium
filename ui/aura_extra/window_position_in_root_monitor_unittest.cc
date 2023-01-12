// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_extra/window_position_in_root_monitor.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace aura_extra {

using WindowPositionInRootMonitorTest = aura::test::AuraTestBase;

TEST_F(WindowPositionInRootMonitorTest, Basic) {
  // Changing the position while not in a root should not notify the callback.
  std::unique_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithId(1, nullptr));
  w1->set_owned_by_parent(false);
  bool monitor_notified = false;
  WindowPositionInRootMonitor monitor(
      w1.get(), base::BindLambdaForTesting([&] { monitor_notified = true; }));
  w1->SetBounds(gfx::Rect(1, 2, 3, 4));
  EXPECT_FALSE(monitor_notified);
  w1->SetBounds(gfx::Rect(11, 2, 3, 4));
  EXPECT_FALSE(monitor_notified);

  // Adding an ancestor that is not part of the root should not notify the
  // callback.
  std::unique_ptr<aura::Window> w2(
      aura::test::CreateTestWindowWithId(2, nullptr));
  w2->set_owned_by_parent(false);
  w2->AddChild(w1.get());
  EXPECT_FALSE(monitor_notified);
  w2->SetBounds(gfx::Rect(21, 10, 20, 20));
  EXPECT_FALSE(monitor_notified);

  // Adding to the root should immediately notify.
  root_window()->AddChild(w2.get());
  EXPECT_TRUE(monitor_notified);
  monitor_notified = false;

  // Changing |w2|'s bounds show notify as |w2| is the parent and |w1| is in a
  // root.
  w2->SetBounds(gfx::Rect(22, 10, 20, 20));
  EXPECT_TRUE(monitor_notified);
  monitor_notified = false;

  // Removing an ancestor, and changing the ancestors bounds should not notify.
  root_window()->RemoveChild(w2.get());
  EXPECT_FALSE(monitor_notified);
  w2->SetBounds(gfx::Rect(21, 22, 23, 24));
  EXPECT_FALSE(monitor_notified);

  // Add |w1| directly to the root, should immediately notify.
  root_window()->AddChild(w1.get());
  EXPECT_TRUE(monitor_notified);
  monitor_notified = false;

  // Changing |w1|s bounds should notify as in a root.
  w1->SetBounds(gfx::Rect(101, 102, 12, 13));
  EXPECT_TRUE(monitor_notified);
  monitor_notified = false;

  // Changing the size should not notify.
  w1->SetBounds(gfx::Rect(101, 102, 121, 13));
  EXPECT_FALSE(monitor_notified);
}

}  // namespace aura_extra

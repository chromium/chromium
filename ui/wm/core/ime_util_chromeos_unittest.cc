// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/ime_util_chromeos.h"

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/insets.h"

namespace wm {

using ImeUtilChromeosTest = aura::test::AuraTestBase;

TEST_F(ImeUtilChromeosTest, RestoreWindowBounds) {
  const gfx::Rect bounds(10, 20, 100, 200);
  aura::Window* window =
      aura::test::CreateTestWindowWithBounds(bounds, root_window());

  EXPECT_EQ(nullptr, window->GetProperty(kVirtualKeyboardRestoreBoundsKey));
  EXPECT_EQ(bounds, window->bounds());

  RestoreWindowBoundsOnClientFocusLost(window);
  EXPECT_EQ(bounds, window->bounds());

  gfx::Rect r1(40, 50, 150, 200);
  window->SetProperty(kVirtualKeyboardRestoreBoundsKey, r1);
  RestoreWindowBoundsOnClientFocusLost(window);
  EXPECT_EQ(r1, window->bounds());
  EXPECT_EQ(nullptr, window->GetProperty(kVirtualKeyboardRestoreBoundsKey));
}

TEST_F(ImeUtilChromeosTest, EnsureWindowNotInRect_NotCovered) {
  const gfx::Rect bounds(0, 0, 100, 200);
  aura::Window* window =
      aura::test::CreateTestWindowWithBounds(bounds, root_window());
  EXPECT_EQ(bounds, window->bounds());
  EXPECT_EQ(bounds, window->GetBoundsInScreen());

  // The rect doesn't overlap on the window.
  gfx::Rect rect(300, 300, 100, 100);
  EXPECT_TRUE(gfx::IntersectRects(window->GetBoundsInScreen(), rect).IsEmpty());
  EnsureWindowNotInRect(window, rect);
  // The bounds should not be changed.
  EXPECT_EQ(bounds, window->bounds());
  EXPECT_EQ(bounds, window->GetBoundsInScreen());
}

TEST_F(ImeUtilChromeosTest, EnsureWindowNotInRect_MoveUp) {
  const gfx::Rect original_bounds(10, 100, 100, 10);
  aura::Window* window =
      aura::test::CreateTestWindowWithBounds(original_bounds, root_window());
  EXPECT_EQ(original_bounds, window->bounds());
  EXPECT_EQ(original_bounds, window->GetBoundsInScreen());

  // The rect overlaps the window. The window is moved up by
  // EnsureWindowNotInRect.
  gfx::Rect rect(50, 50, 200, 200);
  EXPECT_FALSE(
      gfx::IntersectRects(window->GetBoundsInScreen(), rect).IsEmpty());
  EnsureWindowNotInRect(window, rect);
  EXPECT_EQ(gfx::Rect(10, 40, 100, 10), window->bounds());
  EXPECT_EQ(gfx::Rect(10, 40, 100, 10), window->GetBoundsInScreen());
}

TEST_F(ImeUtilChromeosTest, EnsureWindowNotInRect_MoveToTop) {
  const gfx::Rect original_bounds(10, 10, 100, 100);
  aura::Window* window =
      aura::test::CreateTestWindowWithBounds(original_bounds, root_window());
  EXPECT_EQ(original_bounds, window->bounds());
  EXPECT_EQ(original_bounds, window->GetBoundsInScreen());

  // The rect overlaps the window. The window is moved up by
  // EnsureWinodwNotInRect, but there is not enough space above the window.
  gfx::Rect rect(50, 50, 200, 200);
  EXPECT_FALSE(
      gfx::IntersectRects(window->GetBoundsInScreen(), rect).IsEmpty());
  EnsureWindowNotInRect(window, rect);
  EXPECT_EQ(gfx::Rect(10, 0, 100, 100), window->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 100, 100), window->GetBoundsInScreen());
  RestoreWindowBoundsOnClientFocusLost(window);

  // Sets a workspace insets to simulate that something (such as docked
  // magnifier) occupies some space on top. Original bounds must be inside
  // the new work area.
  constexpr int kOccupiedTopHeight = 5;
  ASSERT_GE(original_bounds.y(), kOccupiedTopHeight);

  test_screen()->SetWorkAreaInsets(
      gfx::Insets::TLBR(kOccupiedTopHeight, 0, 0, 0));
  EnsureWindowNotInRect(window, rect);
  EXPECT_EQ(gfx::Rect(10, kOccupiedTopHeight, 100, 100), window->bounds());
}

TEST_F(ImeUtilChromeosTest, MoveUpThenRestore) {
  const gfx::Rect original_bounds(50, 50, 100, 100);
  aura::Window* window =
      aura::test::CreateTestWindowWithBounds(original_bounds, root_window());
  EXPECT_EQ(original_bounds, window->bounds());
  EXPECT_EQ(original_bounds, window->GetBoundsInScreen());

  // EnsureWindowNotInRect moves up the window.
  gfx::Rect rect(50, 50, 200, 200);
  EXPECT_FALSE(
      gfx::IntersectRects(window->GetBoundsInScreen(), rect).IsEmpty());
  EnsureWindowNotInRect(window, rect);
  EXPECT_EQ(gfx::Rect(50, 0, 100, 100), window->bounds());
  EXPECT_EQ(gfx::Rect(50, 0, 100, 100), window->GetBoundsInScreen());

  // The new rect doesn't overlap the moved window bounds, but still overlaps
  // the original window bounds.
  rect = gfx::Rect(50, 120, 200, 200);
  EXPECT_FALSE(gfx::IntersectRects(rect, original_bounds).IsEmpty());
  EnsureWindowNotInRect(window, rect);
  EXPECT_EQ(gfx::Rect(50, 20, 100, 100), window->bounds());
  EXPECT_EQ(gfx::Rect(50, 20, 100, 100), window->GetBoundsInScreen());

  // Now the rect doesn't overlap the original window bounds. The original
  // window bounds should be restored.
  rect = gfx::Rect(200, 200, 200, 200);
  EXPECT_TRUE(gfx::IntersectRects(rect, original_bounds).IsEmpty());
  EnsureWindowNotInRect(window, rect);
  EXPECT_EQ(original_bounds, window->bounds());
  EXPECT_EQ(original_bounds, window->GetBoundsInScreen());
}

}  // namespace wm

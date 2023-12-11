// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_manager.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/test/testing_cursor_client_observer.h"

namespace {

class TestingCursorManager : public wm::NativeCursorManager {
 public:
  // Overridden from wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {}

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitCursor(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitMouseEventsEnabled(enabled);
  }

  void SetCursorSize(ui::CursorSize cursor_size,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitCursorSize(cursor_size);
  }
};

}  // namespace

class CursorManagerTest : public aura::test::AuraTestBase {
 protected:
  CursorManagerTest()
      : cursor_manager_(std::make_unique<TestingCursorManager>()) {}

  wm::CursorManager cursor_manager_;
};

TEST_F(CursorManagerTest, ShowHideCursor) {
  cursor_manager_.SetCursor(ui::mojom::CursorType::kCopy);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  // Check if cursor visibility is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  // Checks setting visiblity while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.HideCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  cursor_manager_.ShowCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  cursor_manager_.ShowCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.HideCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
}

// Verifies that LockCursor/UnlockCursor work correctly with
// EnableMouseEvents and DisableMouseEvents
TEST_F(CursorManagerTest, EnableDisableMouseEvents) {
  cursor_manager_.SetCursor(ui::mojom::CursorType::kCopy);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  // Check if cursor enable state is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // Checks enabling cursor while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.DisableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

TEST_F(CursorManagerTest, SetCursorSize) {
  wm::TestingCursorClientObserver observer;
  cursor_manager_.AddObserver(&observer);

  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kNormal, observer.cursor_size());

  cursor_manager_.SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kNormal, observer.cursor_size());

  cursor_manager_.SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_EQ(ui::CursorSize::kLarge, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kLarge, observer.cursor_size());

  cursor_manager_.SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kNormal, observer.cursor_size());
}

TEST_F(CursorManagerTest, IsMouseEventsEnabled) {
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that the mouse events enable state changes correctly when
// ShowCursor/HideCursor and EnableMouseEvents/DisableMouseEvents are used
// together.
TEST_F(CursorManagerTest, ShowAndEnable) {
  // Changing the visibility of the cursor does not affect the enable state.
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, it also gets invisible.
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are enabled, it restores the visibility state.
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.ShowCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.HideCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, ShowCursor is ignored.
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that calling DisableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
// This is a regression test for http://crbug.com/169404.
TEST_F(CursorManagerTest, MultipleDisableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}

// Verifies that calling EnableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
TEST_F(CursorManagerTest, MultipleEnableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}

TEST_F(CursorManagerTest, TestCursorClientObserver) {
  cursor_manager_.SetCursor(ui::mojom::CursorType::kPointer);
  // Add two observers. Both should have OnCursorVisibilityChanged()
  // invoked when the visibility of the cursor changes.
  wm::TestingCursorClientObserver observer_a;
  wm::TestingCursorClientObserver observer_b;
  cursor_manager_.AddObserver(&observer_a);
  cursor_manager_.AddObserver(&observer_b);

  // Initial state before any events have been sent.
  observer_a.reset();
  observer_b.reset();
  EXPECT_FALSE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());
  EXPECT_FALSE(observer_a.did_cursor_size_change());
  EXPECT_FALSE(observer_b.did_cursor_size_change());

  // Hide the cursor using HideCursor().
  cursor_manager_.HideCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());

  // Set the cursor size.
  cursor_manager_.SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_TRUE(observer_a.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kLarge, observer_a.cursor_size());
  EXPECT_TRUE(observer_b.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kLarge, observer_b.cursor_size());

  // Show the cursor using ShowCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());
  EXPECT_TRUE(observer_b.is_cursor_visible());

  // Remove observer_b. Its OnCursorVisibilityChanged() should
  // not be invoked past this point.
  cursor_manager_.RemoveObserver(&observer_b);

  // Hide the cursor using HideCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.HideCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // Set back the cursor set to normal.
  cursor_manager_.SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_TRUE(observer_a.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kNormal, observer_a.cursor_size());
  EXPECT_FALSE(observer_b.did_cursor_size_change());

  // Show the cursor using ShowCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());

  // Hide the cursor by changing the cursor type.
  cursor_manager_.SetCursor(ui::mojom::CursorType::kPointer);
  observer_a.reset();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kNone);
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // Show the cursor by changing the cursor type.
  observer_a.reset();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kPointer);
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());

  // Changing the type to another visible type doesn't cause unnecessary
  // callbacks.
  observer_a.reset();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kHand);
  EXPECT_FALSE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // If the type is kNone, showing the cursor shouldn't cause observers to
  // think that the cursor is now visible.
  cursor_manager_.HideCursor();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kNone);
  observer_a.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
}

// This test validates that the cursor visiblity state is restored when a
// CursorManager instance is destroyed and recreated.
TEST(CursorManagerCreateDestroyTest, VisibilityTest) {
  // This block ensures that the cursor is hidden when the CursorManager
  // instance is destroyed.
  {
    wm::CursorManager cursor_manager1(
        base::WrapUnique(new TestingCursorManager));
    cursor_manager1.ShowCursor();
    EXPECT_TRUE(cursor_manager1.IsCursorVisible());
    cursor_manager1.HideCursor();
    EXPECT_FALSE(cursor_manager1.IsCursorVisible());
  }

  // This block validates that the cursor is hidden initially. It ensures that
  // the cursor is visible when the CursorManager instance is destroyed.
  {
    wm::CursorManager cursor_manager2(
        base::WrapUnique(new TestingCursorManager));
    EXPECT_FALSE(cursor_manager2.IsCursorVisible());
    cursor_manager2.ShowCursor();
    EXPECT_TRUE(cursor_manager2.IsCursorVisible());
  }

  // This block validates that the cursor is visible initially. It then
  // performs normal cursor visibility operations.
  {
    wm::CursorManager cursor_manager3(
        base::WrapUnique(new TestingCursorManager));
    EXPECT_TRUE(cursor_manager3.IsCursorVisible());
    cursor_manager3.HideCursor();
    EXPECT_FALSE(cursor_manager3.IsCursorVisible());
  }
}

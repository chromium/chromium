// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/scoped_paint_lock.h"

#include <memory>

#include "cc/paint/display_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/paint_context.h"
#include "ui/views/paint_info.h"
#include "ui/views/view.h"
#include "ui/views/view_test_api.h"

namespace views {

namespace {

// We use a custom view that records how many times OnDidSchedulePaint
// was called. Because ScopedPaintLock defers propagation to the parent,
// inspecting the parent's OnDidSchedulePaint accurately measures whether
// a paint actually escaped the locked sub-tree.
class TestView : public View {
  METADATA_HEADER(TestView, View)

 public:
  TestView() { SetVisible(true); }
  ~TestView() override = default;

  void OnDidSchedulePaint(const gfx::Rect& rect) override {
    paint_scheduled_count_++;
  }

  void OnPaint(gfx::Canvas* canvas) override {
    painted_count_++;
    View::OnPaint(canvas);
  }

  int paint_scheduled_count() const { return paint_scheduled_count_; }
  void reset_paint_scheduled_count() { paint_scheduled_count_ = 0; }

  int painted_count() const { return painted_count_; }
  void reset_painted_count() { painted_count_ = 0; }

 private:
  int paint_scheduled_count_ = 0;
  int painted_count_ = 0;
};

BEGIN_METADATA(TestView)
END_METADATA

}  // namespace

using ScopedPaintLockTest = testing::Test;

TEST_F(ScopedPaintLockTest, DefersPaintWhileLocked) {
  TestView parent;
  TestView* view = parent.AddChildView(std::make_unique<TestView>());

  auto paint_lock = std::make_unique<ScopedPaintLock>(view);

  view->SchedulePaint();
  EXPECT_TRUE(ViewTestApi(view).paint_pending_while_locked());
  EXPECT_EQ(0, parent.paint_scheduled_count());

  // Removing the lock should trigger a deferred paint and propagate to parent.
  paint_lock.reset();
  EXPECT_FALSE(ViewTestApi(view).paint_pending_while_locked());
  EXPECT_EQ(1, parent.paint_scheduled_count());
}

TEST_F(ScopedPaintLockTest, MultipleLocks) {
  TestView parent;
  TestView* view = parent.AddChildView(std::make_unique<TestView>());

  auto lock1 = std::make_unique<ScopedPaintLock>(view);
  auto lock2 = std::make_unique<ScopedPaintLock>(view);

  view->SchedulePaint();
  EXPECT_TRUE(ViewTestApi(view).paint_pending_while_locked());
  EXPECT_EQ(0, parent.paint_scheduled_count());

  // Removing the first lock shouldn't trigger paint.
  lock1.reset();
  EXPECT_TRUE(ViewTestApi(view).paint_pending_while_locked());
  EXPECT_EQ(0, parent.paint_scheduled_count());

  // Removing the last lock should trigger paint.
  lock2.reset();
  EXPECT_FALSE(ViewTestApi(view).paint_pending_while_locked());
  EXPECT_EQ(1, parent.paint_scheduled_count());
}

TEST_F(ScopedPaintLockTest, SafeOnViewDeletion) {
  auto view = std::make_unique<TestView>();
  auto lock = std::make_unique<ScopedPaintLock>(view.get());

  // Deleting the view before the lock should not crash.
  view.reset();

  // Releasing the lock should be a no-op and not crash.
  lock.reset();
}

TEST_F(ScopedPaintLockTest, ViewTree) {
  TestView parent;
  TestView* root = parent.AddChildView(std::make_unique<TestView>());
  TestView* child1 = root->AddChildView(std::make_unique<TestView>());
  TestView* grandchild1 = child1->AddChildView(std::make_unique<TestView>());
  TestView* child2 = root->AddChildView(std::make_unique<TestView>());
  TestView* grandchild2 = child2->AddChildView(std::make_unique<TestView>());

  auto root_lock = std::make_unique<ScopedPaintLock>(root);
  auto child1_lock = std::make_unique<ScopedPaintLock>(child1);

  grandchild1->SchedulePaint();
  grandchild2->SchedulePaint();

  EXPECT_TRUE(ViewTestApi(grandchild1).paint_pending_while_locked());
  EXPECT_TRUE(ViewTestApi(grandchild2).paint_pending_while_locked());
  EXPECT_EQ(0, parent.paint_scheduled_count());

  // Removing child1_lock doesn't propagate paint because root is still locked.
  child1_lock.reset();

  EXPECT_TRUE(ViewTestApi(grandchild1).paint_pending_while_locked());
  EXPECT_TRUE(ViewTestApi(grandchild2).paint_pending_while_locked());
  EXPECT_EQ(0, parent.paint_scheduled_count());

  // Removing root_lock should unlock the whole tree.
  // Both grandchild1 and grandchild2 will schedule paint, so parent will see 2
  // calls.
  root_lock.reset();

  EXPECT_FALSE(ViewTestApi(grandchild1).paint_pending_while_locked());
  EXPECT_FALSE(ViewTestApi(grandchild2).paint_pending_while_locked());
  EXPECT_EQ(2, parent.paint_scheduled_count());
}

TEST_F(ScopedPaintLockTest, BoundsChangeWhileLocked) {
  TestView parent;
  TestView* view = parent.AddChildView(std::make_unique<TestView>());

  auto lock = std::make_unique<ScopedPaintLock>(view);

  // Changing bounds should schedule paint twice (once for old bounds, once for
  // new), which should be deferred by the lock.
  view->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(ViewTestApi(view).paint_pending_while_locked());
  EXPECT_EQ(0, parent.paint_scheduled_count());

  // Removing the lock should trigger the deferred paint once.
  lock.reset();
  EXPECT_FALSE(ViewTestApi(view).paint_pending_while_locked());
  EXPECT_EQ(1, parent.paint_scheduled_count());
}

TEST_F(ScopedPaintLockTest, OnPaintNotCalledWhileLocked) {
  TestView view;
  view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  auto lock = std::make_unique<ScopedPaintLock>(&view);

  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  view.Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, /*invalidation=*/gfx::Rect(100, 100),
                       false),
      view.size()));

  EXPECT_EQ(0, view.painted_count());

  lock.reset();

  view.Paint(PaintInfo::CreateRootPaintInfo(
      ui::PaintContext(list.get(), 1.f, /*invalidation=*/gfx::Rect(100, 100),
                       false),
      view.size()));

  EXPECT_EQ(1, view.painted_count());
}

TEST_F(ScopedPaintLockTest, OnDidSchedulePaintNotCalledWhileLocked) {
  TestView view;

  auto lock = std::make_unique<ScopedPaintLock>(&view);

  view.SchedulePaint();
  EXPECT_TRUE(ViewTestApi(&view).paint_pending_while_locked());

  // The locked view itself should not receive OnDidSchedulePaint while locked.
  EXPECT_EQ(0, view.paint_scheduled_count());

  // Removing the lock should trigger the deferred paint.
  lock.reset();
  EXPECT_FALSE(ViewTestApi(&view).paint_pending_while_locked());
  EXPECT_EQ(1, view.paint_scheduled_count());
}

}  // namespace views

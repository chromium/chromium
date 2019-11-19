// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager_base.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_layout_manager.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace views {

namespace {

constexpr int kChildViewPadding = 5;
constexpr gfx::Size kMinimumSize(40, 50);
constexpr gfx::Size kPreferredSize(100, 90);
constexpr gfx::Size kSquarishSize(10, 11);
constexpr gfx::Size kLongSize(20, 8);
constexpr gfx::Size kTallSize(4, 22);
constexpr gfx::Size kLargeSize(30, 28);

// Dummy class that minimally implements LayoutManagerBase for basic
// functionality testing.
class TestLayoutManagerBase : public LayoutManagerBase {
 public:
  std::vector<const View*> GetIncludedChildViews() const {
    std::vector<const View*> included;
    std::copy_if(host_view()->children().begin(), host_view()->children().end(),
                 std::back_inserter(included), [=](const View* child) {
                   return IsChildIncludedInLayout(child);
                 });
    return included;
  }

  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override {
    ProposedLayout layout;
    layout.host_size.set_width(
        std::max(kMinimumSize.width(),
                 size_bounds.width().value_or(kPreferredSize.width())));
    layout.host_size.set_height(
        std::max(kMinimumSize.height(),
                 size_bounds.height().value_or(kPreferredSize.height())));
    return layout;
  }
};

// This layout layout lays out included child views in the upper-left of the
// host view with kChildViewPadding around them. Views that will not fit are
// made invisible. Child views are expected to overlap as they all have the
// same top-left corner.
class MockLayoutManagerBase : public LayoutManagerBase {
 public:
  using LayoutManagerBase::AddOwnedLayout;
  using LayoutManagerBase::InvalidateHost;

  int num_invalidations() const { return num_invalidations_; }
  int num_layouts_generated() const { return num_layouts_generated_; }

  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override {
    ProposedLayout layout;
    layout.host_size = {kChildViewPadding, kChildViewPadding};
    for (auto it = host_view()->children().begin();
         it != host_view()->children().end(); ++it) {
      if (!IsChildIncludedInLayout(*it))
        continue;
      const gfx::Size preferred_size = (*it)->GetPreferredSize();
      bool visible = false;
      gfx::Rect bounds;
      const int required_width = preferred_size.width() + 2 * kChildViewPadding;
      const int required_height =
          preferred_size.height() + 2 * kChildViewPadding;
      if ((!size_bounds.width() || required_width <= *size_bounds.width()) &&
          (!size_bounds.height() || required_height <= *size_bounds.height())) {
        visible = true;
        bounds = gfx::Rect(kChildViewPadding, kChildViewPadding,
                           preferred_size.width(), preferred_size.height());
        layout.host_size.set_width(std::max(
            layout.host_size.width(), bounds.right() + kChildViewPadding));
        layout.host_size.set_height(std::max(
            layout.host_size.height(), bounds.bottom() + kChildViewPadding));
      }
      layout.child_layouts.push_back({*it, visible, bounds});
    }
    ++num_layouts_generated_;
    return layout;
  }

  void OnLayoutChanged() override {
    LayoutManagerBase::OnLayoutChanged();
    ++num_invalidations_;
  }

  using LayoutManagerBase::ApplyLayout;

 private:
  mutable int num_layouts_generated_ = 0;
  mutable int num_invalidations_ = 0;
};

void ExpectSameViews(const std::vector<const View*>& expected,
                     const std::vector<const View*>& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

}  // namespace

TEST(LayoutManagerBaseTest, GetMinimumSize) {
  TestLayoutManagerBase layout;
  EXPECT_EQ(kMinimumSize, layout.GetMinimumSize(nullptr));
}

TEST(LayoutManagerBaseTest, GetPreferredSize) {
  TestLayoutManagerBase layout;
  EXPECT_EQ(kPreferredSize, layout.GetPreferredSize(nullptr));
}

TEST(LayoutManagerBaseTest, GetPreferredHeightForWidth) {
  constexpr int kWidth = 45;
  TestLayoutManagerBase layout;
  EXPECT_EQ(kPreferredSize.height(),
            layout.GetPreferredHeightForWidth(nullptr, kWidth));
}

TEST(LayoutManagerBaseTest, Installed) {
  auto layout_ptr = std::make_unique<TestLayoutManagerBase>();
  LayoutManagerBase* layout = layout_ptr.get();
  EXPECT_EQ(nullptr, layout->host_view());

  View view;
  view.SetLayoutManager(std::move(layout_ptr));
  EXPECT_EQ(&view, layout->host_view());
}

TEST(LayoutManagerBaseTest, SetChildIncludedInLayout) {
  View view;
  View* const child1 = view.AddChildView(std::make_unique<View>());
  View* const child2 = view.AddChildView(std::make_unique<View>());
  View* const child3 = view.AddChildView(std::make_unique<View>());

  auto layout_ptr = std::make_unique<TestLayoutManagerBase>();
  TestLayoutManagerBase* layout = layout_ptr.get();
  view.SetLayoutManager(std::move(layout_ptr));

  // All views should be present.
  ExpectSameViews({child1, child2, child3}, layout->GetIncludedChildViews());

  // Remove one.
  layout->SetChildViewIgnoredByLayout(child2, true);
  ExpectSameViews({child1, child3}, layout->GetIncludedChildViews());

  // Remove another.
  layout->SetChildViewIgnoredByLayout(child1, true);
  ExpectSameViews({child3}, layout->GetIncludedChildViews());

  // Removing it again should have no effect.
  layout->SetChildViewIgnoredByLayout(child1, true);
  ExpectSameViews({child3}, layout->GetIncludedChildViews());

  // Add one back.
  layout->SetChildViewIgnoredByLayout(child1, false);
  ExpectSameViews({child1, child3}, layout->GetIncludedChildViews());

  // Adding it back again should have no effect.
  layout->SetChildViewIgnoredByLayout(child1, false);
  ExpectSameViews({child1, child3}, layout->GetIncludedChildViews());

  // Add the other view back.
  layout->SetChildViewIgnoredByLayout(child2, false);
  ExpectSameViews({child1, child2, child3}, layout->GetIncludedChildViews());
}

TEST(LayoutManagerBaseTest, InvalidateHost_NotInstalled) {
  MockLayoutManagerBase root_layout;
  MockLayoutManagerBase* const child1 =
      root_layout.AddOwnedLayout(std::make_unique<MockLayoutManagerBase>());
  MockLayoutManagerBase* const child2 =
      root_layout.AddOwnedLayout(std::make_unique<MockLayoutManagerBase>());
  MockLayoutManagerBase* const grandchild =
      child1->AddOwnedLayout(std::make_unique<MockLayoutManagerBase>());

  root_layout.InvalidateHost(false);
  EXPECT_EQ(0, root_layout.num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  child1->InvalidateHost(false);
  EXPECT_EQ(0, root_layout.num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  child2->InvalidateHost(false);
  EXPECT_EQ(0, root_layout.num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  grandchild->InvalidateHost(false);
  EXPECT_EQ(0, root_layout.num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  root_layout.InvalidateHost(true);
  EXPECT_EQ(1, root_layout.num_invalidations());
  EXPECT_EQ(1, child1->num_invalidations());
  EXPECT_EQ(1, child2->num_invalidations());
  EXPECT_EQ(1, grandchild->num_invalidations());

  child1->InvalidateHost(true);
  EXPECT_EQ(2, root_layout.num_invalidations());
  EXPECT_EQ(2, child1->num_invalidations());
  EXPECT_EQ(2, child2->num_invalidations());
  EXPECT_EQ(2, grandchild->num_invalidations());

  child2->InvalidateHost(true);
  EXPECT_EQ(3, root_layout.num_invalidations());
  EXPECT_EQ(3, child1->num_invalidations());
  EXPECT_EQ(3, child2->num_invalidations());
  EXPECT_EQ(3, grandchild->num_invalidations());

  grandchild->InvalidateHost(true);
  EXPECT_EQ(4, root_layout.num_invalidations());
  EXPECT_EQ(4, child1->num_invalidations());
  EXPECT_EQ(4, child2->num_invalidations());
  EXPECT_EQ(4, grandchild->num_invalidations());
}

TEST(LayoutManagerBaseTest, InvalidateHost_Installed) {
  View view;
  MockLayoutManagerBase* const root_layout =
      view.SetLayoutManager(std::make_unique<MockLayoutManagerBase>());
  MockLayoutManagerBase* const child1 =
      root_layout->AddOwnedLayout(std::make_unique<MockLayoutManagerBase>());
  MockLayoutManagerBase* const child2 =
      root_layout->AddOwnedLayout(std::make_unique<MockLayoutManagerBase>());
  MockLayoutManagerBase* const grandchild =
      child1->AddOwnedLayout(std::make_unique<MockLayoutManagerBase>());

  root_layout->InvalidateHost(false);
  EXPECT_EQ(0, root_layout->num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  child1->InvalidateHost(false);
  EXPECT_EQ(0, root_layout->num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  child2->InvalidateHost(false);
  EXPECT_EQ(0, root_layout->num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  grandchild->InvalidateHost(false);
  EXPECT_EQ(0, root_layout->num_invalidations());
  EXPECT_EQ(0, child1->num_invalidations());
  EXPECT_EQ(0, child2->num_invalidations());
  EXPECT_EQ(0, grandchild->num_invalidations());

  root_layout->InvalidateHost(true);
  EXPECT_EQ(1, root_layout->num_invalidations());
  EXPECT_EQ(1, child1->num_invalidations());
  EXPECT_EQ(1, child2->num_invalidations());
  EXPECT_EQ(1, grandchild->num_invalidations());

  child1->InvalidateHost(true);
  EXPECT_EQ(2, root_layout->num_invalidations());
  EXPECT_EQ(2, child1->num_invalidations());
  EXPECT_EQ(2, child2->num_invalidations());
  EXPECT_EQ(2, grandchild->num_invalidations());

  child2->InvalidateHost(true);
  EXPECT_EQ(3, root_layout->num_invalidations());
  EXPECT_EQ(3, child1->num_invalidations());
  EXPECT_EQ(3, child2->num_invalidations());
  EXPECT_EQ(3, grandchild->num_invalidations());

  grandchild->InvalidateHost(true);
  EXPECT_EQ(4, root_layout->num_invalidations());
  EXPECT_EQ(4, child1->num_invalidations());
  EXPECT_EQ(4, child2->num_invalidations());
  EXPECT_EQ(4, grandchild->num_invalidations());
}

// Test LayoutManager functionality of LayoutManagerBase:

namespace {

// Base for tests that evaluate the LayoutManager functionality of
// LayoutManagerBase (rather than the LayoutManagerBase-specific behavior).
class LayoutManagerBaseManagerTest : public testing::Test {
 public:
  void SetUp() override {
    host_view_ = std::make_unique<View>();
    layout_manager_ =
        host_view_->SetLayoutManager(std::make_unique<MockLayoutManagerBase>());
  }

  View* AddChildView(gfx::Size preferred_size) {
    auto child = std::make_unique<StaticSizedView>(preferred_size);
    return host_view_->AddChildView(std::move(child));
  }

  View* host_view() { return host_view_.get(); }
  MockLayoutManagerBase* layout_manager() { return layout_manager_; }
  View* child(int index) { return host_view_->children().at(index); }

 private:
  std::unique_ptr<View> host_view_;
  MockLayoutManagerBase* layout_manager_;
};

}  // namespace

TEST_F(LayoutManagerBaseManagerTest, ApplyLayout) {
  AddChildView(gfx::Size());
  AddChildView(gfx::Size());
  AddChildView(gfx::Size());

  // We don't want to set the size of the host view because it will trigger a
  // superfluous layout, so we'll just keep the old size and make sure it
  // doesn't change.
  const gfx::Size old_size = host_view()->size();

  ProposedLayout layout;
  // This should be ignored.
  layout.host_size = {123, 456};

  // Set the child visibility and bounds.
  constexpr gfx::Rect kChild1Bounds(3, 4, 10, 15);
  constexpr gfx::Rect kChild3Bounds(20, 21, 12, 14);
  layout.child_layouts.push_back({child(0), true, kChild1Bounds});
  layout.child_layouts.push_back({child(1), false});
  layout.child_layouts.push_back({child(2), true, kChild3Bounds});

  layout_manager()->ApplyLayout(layout);

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(kChild1Bounds, child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(kChild3Bounds, child(2)->bounds());
  EXPECT_EQ(old_size, host_view()->size());
}

TEST_F(LayoutManagerBaseManagerTest, ApplyLayout_SkipsOmittedViews) {
  AddChildView(gfx::Size());
  AddChildView(gfx::Size());
  AddChildView(gfx::Size());

  ProposedLayout layout;
  // Set the child visibility and bounds.
  constexpr gfx::Rect kChild1Bounds(3, 4, 10, 15);
  constexpr gfx::Rect kChild2Bounds(1, 2, 3, 4);
  layout.child_layouts.push_back({child(0), true, kChild1Bounds});
  layout.child_layouts.push_back({child(2), false});

  // We'll set the second child separately.
  child(1)->SetVisible(true);
  child(1)->SetBoundsRect(kChild2Bounds);

  layout_manager()->ApplyLayout(layout);

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(kChild1Bounds, child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(kChild2Bounds, child(1)->bounds());
  EXPECT_FALSE(child(2)->GetVisible());
}

TEST_F(LayoutManagerBaseManagerTest, Install) {
  EXPECT_EQ(host_view(), layout_manager()->host_view());
}

TEST_F(LayoutManagerBaseManagerTest, GetMinimumSize) {
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);
  EXPECT_EQ(gfx::Size(kChildViewPadding, kChildViewPadding),
            host_view()->GetMinimumSize());
}

TEST_F(LayoutManagerBaseManagerTest, GetPreferredSize) {
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);
  const gfx::Size expected(kLongSize.width() + 2 * kChildViewPadding,
                           kTallSize.height() + 2 * kChildViewPadding);
  EXPECT_EQ(expected, host_view()->GetPreferredSize());
}

TEST_F(LayoutManagerBaseManagerTest, GetPreferredHeightForWidth) {
  AddChildView(kSquarishSize);
  AddChildView(kLargeSize);
  const int expected = kSquarishSize.height() + 2 * kChildViewPadding;
  EXPECT_EQ(expected,
            layout_manager()->GetPreferredHeightForWidth(host_view(), 20));
  EXPECT_EQ(1, layout_manager()->num_layouts_generated());
  layout_manager()->GetPreferredHeightForWidth(host_view(), 20);
  EXPECT_EQ(1, layout_manager()->num_layouts_generated());
  layout_manager()->GetPreferredHeightForWidth(host_view(), 25);
  EXPECT_EQ(2, layout_manager()->num_layouts_generated());
}

TEST_F(LayoutManagerBaseManagerTest, InvalidateLayout) {
  // Some invalidation could have been triggered during setup.
  const int old_num_invalidations = layout_manager()->num_invalidations();

  host_view()->InvalidateLayout();
  EXPECT_EQ(old_num_invalidations + 1, layout_manager()->num_invalidations());
}

TEST_F(LayoutManagerBaseManagerTest, Layout) {
  constexpr gfx::Point kUpperLeft(kChildViewPadding, kChildViewPadding);
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  // This should fit all of the child views and trigger layout.
  host_view()->SetSize({40, 40});
  EXPECT_EQ(1, layout_manager()->num_layouts_generated());
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(0)->GetPreferredSize()),
            child(0)->bounds());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(1)->GetPreferredSize()),
            child(1)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(2)->GetPreferredSize()),
            child(2)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());

  // This should drop out some children.
  host_view()->SetSize({25, 25});
  EXPECT_EQ(2, layout_manager()->num_layouts_generated());
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(0)->GetPreferredSize()),
            child(0)->bounds());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());
}

TEST_F(LayoutManagerBaseManagerTest, ChildViewIgnoredByLayout) {
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  EXPECT_FALSE(layout_manager()->IsChildViewIgnoredByLayout(child(0)));
  EXPECT_FALSE(layout_manager()->IsChildViewIgnoredByLayout(child(1)));
  EXPECT_FALSE(layout_manager()->IsChildViewIgnoredByLayout(child(2)));

  layout_manager()->SetChildViewIgnoredByLayout(child(1), true);

  EXPECT_FALSE(layout_manager()->IsChildViewIgnoredByLayout(child(0)));
  EXPECT_TRUE(layout_manager()->IsChildViewIgnoredByLayout(child(1)));
  EXPECT_FALSE(layout_manager()->IsChildViewIgnoredByLayout(child(2)));
}

TEST_F(LayoutManagerBaseManagerTest,
       ChildViewIgnoredByLayout_IgnoresChildView) {
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  layout_manager()->SetChildViewIgnoredByLayout(child(1), true);

  child(1)->SetSize(kLargeSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_EQ(child(0)->GetPreferredSize(), child(0)->size());
  EXPECT_EQ(kLargeSize, child(1)->size());
  EXPECT_EQ(child(2)->GetPreferredSize(), child(2)->size());
}

TEST_F(LayoutManagerBaseManagerTest, ViewVisibilitySet) {
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  child(1)->SetVisible(false);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize(), child(0)->size());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(child(2)->GetPreferredSize(), child(2)->size());

  // Turn the second child view back on and verify it's present in the layout
  // again.
  child(1)->SetVisible(true);
  host_view()->Layout();

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize(), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize(), child(1)->size());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(child(2)->GetPreferredSize(), child(2)->size());
}

TEST_F(LayoutManagerBaseManagerTest, ViewAdded) {
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize(), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize(), child(1)->size());

  // Add a new view and verify it is being laid out.
  View* new_view = AddChildView(kSquarishSize);
  host_view()->Layout();

  EXPECT_TRUE(new_view->GetVisible());
  EXPECT_EQ(new_view->GetPreferredSize(), new_view->size());
}

TEST_F(LayoutManagerBaseManagerTest, ViewAdded_NotVisible) {
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize(), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize(), child(1)->size());

  // Add a new view that is not visible and ensure that the layout manager
  // doesn't touch it during layout.
  View* new_view = new StaticSizedView(kSquarishSize);
  new_view->SetVisible(false);
  host_view()->AddChildView(new_view);
  host_view()->Layout();

  EXPECT_FALSE(new_view->GetVisible());
}

TEST_F(LayoutManagerBaseManagerTest, ViewRemoved) {
  AddChildView(kSquarishSize);
  View* const child_view = AddChildView(kLongSize);
  AddChildView(kTallSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize(), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize(), child(1)->size());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(child(2)->GetPreferredSize(), child(2)->size());

  host_view()->RemoveChildView(child_view);
  child_view->SetSize(kLargeSize);
  host_view()->Layout();

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize(), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize(), child(1)->size());

  EXPECT_TRUE(child_view->GetVisible());
  EXPECT_EQ(kLargeSize, child_view->size());

  // Required since we removed it from the parent view.
  delete child_view;
}

TEST(LayoutManagerBase_ProposedLayoutTest, Equality) {
  View* ptr0 = nullptr;
  View* ptr1 = ptr0 + 1;
  View* ptr2 = ptr0 + 2;
  ProposedLayout a;
  ProposedLayout b;
  EXPECT_TRUE(a == b);
  a.host_size = {1, 2};
  EXPECT_FALSE(a == b);
  b.host_size = {1, 2};
  EXPECT_TRUE(a == b);
  a.child_layouts.push_back({ptr0, true, {1, 1, 2, 2}});
  EXPECT_FALSE(a == b);
  b.child_layouts.push_back(a.child_layouts[0]);
  EXPECT_TRUE(a == b);
  a.child_layouts[0].visible = false;
  EXPECT_FALSE(a == b);
  b.child_layouts[0].visible = false;
  EXPECT_TRUE(a == b);
  b.child_layouts[0].bounds = {0, 0, 3, 3};
  // Since |visible| == false, changing bounds doesn't change anything.
  EXPECT_TRUE(a == b);
  a.child_layouts[0].visible = true;
  b.child_layouts[0].visible = true;
  EXPECT_FALSE(a == b);
  a.child_layouts[0].visible = false;
  b.child_layouts[0].visible = false;
  a.child_layouts.push_back({ptr1, true, {1, 2, 3, 4}});
  b.child_layouts.push_back({ptr2, true, {1, 2, 3, 4}});
  EXPECT_FALSE(a == b);
  b.child_layouts[1].child_view = ptr1;
  EXPECT_TRUE(a == b);
}

}  // namespace views

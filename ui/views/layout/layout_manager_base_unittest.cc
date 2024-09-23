// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager_base.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

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
    base::ranges::copy_if(host_view()->children(), std::back_inserter(included),
                          [=, this](const View* child) {
                            return IsChildIncludedInLayout(child);
                          });
    return included;
  }

  void OverrideProposedLayout(const ProposedLayout& forced_layout) {
    forced_layout_ = forced_layout;
    InvalidateHost(true);
  }

  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override {
    if (forced_layout_)
      return *forced_layout_;

    ProposedLayout layout;
    layout.host_size.set_width(std::clamp<SizeBound>(size_bounds.width(),
                                                     kMinimumSize.width(),
                                                     kPreferredSize.width())
                                   .value());
    layout.host_size.set_height(std::clamp<SizeBound>(size_bounds.height(),
                                                      kMinimumSize.height(),
                                                      kPreferredSize.height())
                                    .value());
    return layout;
  }

  void LayoutImpl() override {
    ++layout_count_;
    LayoutManagerBase::LayoutImpl();
  }

  size_t layout_count() const { return layout_count_; }

 private:
  // If specified, will always return this layout.
  std::optional<ProposedLayout> forced_layout_;

  size_t layout_count_ = 0;
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
    for (views::View* it : host_view()->children()) {
      if (!IsChildIncludedInLayout(it))
        continue;
      const gfx::Size preferred_size = it->GetPreferredSize({});
      bool visible = false;
      gfx::Rect bounds;
      const int required_width = preferred_size.width() + 2 * kChildViewPadding;
      const int required_height =
          preferred_size.height() + 2 * kChildViewPadding;
      if ((required_width <= size_bounds.width()) &&
          (required_height <= size_bounds.height())) {
        visible = true;
        bounds = gfx::Rect(kChildViewPadding, kChildViewPadding,
                           preferred_size.width(), preferred_size.height());
        layout.host_size.set_width(std::max(
            layout.host_size.width(), bounds.right() + kChildViewPadding));
        layout.host_size.set_height(std::max(
            layout.host_size.height(), bounds.bottom() + kChildViewPadding));
      }
      layout.child_layouts.push_back({it, visible, bounds});
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
  child2->SetProperty(kViewIgnoredByLayoutKey, true);
  ExpectSameViews({child1, child3}, layout->GetIncludedChildViews());

  // Remove another.
  child1->SetProperty(kViewIgnoredByLayoutKey, true);
  ExpectSameViews({child3}, layout->GetIncludedChildViews());

  // Removing it again should have no effect.
  child1->SetProperty(kViewIgnoredByLayoutKey, true);
  ExpectSameViews({child3}, layout->GetIncludedChildViews());

  // Add one back.
  child1->SetProperty(kViewIgnoredByLayoutKey, false);
  ExpectSameViews({child1, child3}, layout->GetIncludedChildViews());

  // Adding it back again should have no effect.
  child1->SetProperty(kViewIgnoredByLayoutKey, false);
  ExpectSameViews({child1, child3}, layout->GetIncludedChildViews());

  // Add the other view back.
  child2->SetProperty(kViewIgnoredByLayoutKey, false);
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

  // Directly calls the layout manager's `Layout()` method, as if from within
  // `View::Layout()` or an overridden version of the function.
  void DoLayoutManagerLayout() { layout_manager_->Layout(host_view_.get()); }

  View* host_view() { return host_view_.get(); }
  MockLayoutManagerBase* layout_manager() { return layout_manager_; }
  View* child(int index) { return host_view_->children().at(index); }

 private:
  std::unique_ptr<View> host_view_;
  raw_ptr<MockLayoutManagerBase> layout_manager_;
};

}  // namespace

TEST_F(LayoutManagerBaseManagerTest, ProposedLayout_GetLayoutFor) {
  AddChildView(gfx::Size());
  AddChildView(gfx::Size());
  AddChildView(gfx::Size());

  ProposedLayout layout;
  constexpr gfx::Rect kChild1Bounds(3, 4, 10, 15);
  layout.child_layouts.push_back({child(0), true, kChild1Bounds});
  layout.child_layouts.push_back({child(1), false});

  const ProposedLayout& const_layout = layout;

  EXPECT_EQ(&layout.child_layouts[0], layout.GetLayoutFor(child(0)));
  EXPECT_EQ(&const_layout.child_layouts[0],
            const_layout.GetLayoutFor(child(0)));
  EXPECT_EQ(&layout.child_layouts[1], layout.GetLayoutFor(child(1)));
  EXPECT_EQ(&const_layout.child_layouts[1],
            const_layout.GetLayoutFor(child(1)));
  EXPECT_EQ(nullptr, layout.GetLayoutFor(child(2)));
  EXPECT_EQ(nullptr, const_layout.GetLayoutFor(child(2)));
}

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
  EXPECT_EQ(expected, host_view()->GetPreferredSize({}));
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
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(0)->GetPreferredSize({})),
            child(0)->bounds());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(1)->GetPreferredSize({})),
            child(1)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(2)->GetPreferredSize({})),
            child(2)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());

  // This should drop out some children.
  host_view()->SetSize({25, 25});
  EXPECT_EQ(2, layout_manager()->num_layouts_generated());
  EXPECT_EQ(gfx::Rect(kUpperLeft, child(0)->GetPreferredSize({})),
            child(0)->bounds());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());
}

TEST_F(LayoutManagerBaseManagerTest, IgnoresChildWithViewIgnoredByLayoutKey) {
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  child(1)->SetProperty(kViewIgnoredByLayoutKey, true);
  child(1)->SetSize(kLargeSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_EQ(child(0)->GetPreferredSize({}), child(0)->size());
  EXPECT_EQ(kLargeSize, child(1)->size());
  EXPECT_EQ(child(2)->GetPreferredSize({}), child(2)->size());
}

TEST_F(LayoutManagerBaseManagerTest, ViewVisibilitySet) {
  AddChildView(kSquarishSize);
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  child(1)->SetVisible(false);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize({}), child(0)->size());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(child(2)->GetPreferredSize({}), child(2)->size());

  // Turn the second child view back on and verify it's present in the layout
  // again.
  child(1)->SetVisible(true);
  test::RunScheduledLayout(host_view());

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize({}), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize({}), child(1)->size());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(child(2)->GetPreferredSize({}), child(2)->size());
}

TEST_F(LayoutManagerBaseManagerTest, ViewAdded) {
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize({}), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize({}), child(1)->size());

  // Add a new view and verify it is being laid out.
  View* new_view = AddChildView(kSquarishSize);
  test::RunScheduledLayout(host_view());

  EXPECT_TRUE(new_view->GetVisible());
  EXPECT_EQ(new_view->GetPreferredSize({}), new_view->size());
}

TEST_F(LayoutManagerBaseManagerTest, ViewAdded_NotVisible) {
  AddChildView(kLongSize);
  AddChildView(kTallSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize({}), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize({}), child(1)->size());

  // Add a new view that is not visible and ensure that the layout manager
  // doesn't touch it during layout.
  View* new_view = new StaticSizedView(kSquarishSize);
  new_view->SetVisible(false);
  host_view()->AddChildView(new_view);
  test::RunScheduledLayout(host_view());

  EXPECT_FALSE(new_view->GetVisible());
}

TEST_F(LayoutManagerBaseManagerTest, ViewRemoved) {
  AddChildView(kSquarishSize);
  View* const child_view = AddChildView(kLongSize);
  AddChildView(kTallSize);

  // Makes enough room for all views, and triggers layout.
  host_view()->SetSize({50, 50});

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize({}), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize({}), child(1)->size());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(child(2)->GetPreferredSize({}), child(2)->size());

  std::unique_ptr<View> owned_child_view =
      host_view()->RemoveChildViewT(child_view);
  owned_child_view->SetSize(kLargeSize);
  test::RunScheduledLayout(host_view());

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(child(0)->GetPreferredSize({}), child(0)->size());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(child(1)->GetPreferredSize({}), child(1)->size());

  EXPECT_TRUE(owned_child_view->GetVisible());
  EXPECT_EQ(kLargeSize, owned_child_view->size());
}

class TestIgnoredView : public View {
  METADATA_HEADER(TestIgnoredView, View)
 public:
  TestIgnoredView() {
    SetProperty(kViewIgnoredByLayoutKey, true);
    SetBoundsRect(kExpectedBounds);
  }

  static constexpr gfx::Rect kExpectedBounds{1, 2, 10, 40};
};

BEGIN_METADATA(TestIgnoredView)
END_METADATA

// An ignored view's visibility and layout are managed outside of the layout
// manager, whether that's in the view itself or in a manual layout method in
// its parent view.
TEST_F(LayoutManagerBaseManagerTest, IgnoredView) {
  View* child_view = AddChildView(kSquarishSize);
  test::RunScheduledLayout(host_view());
  gfx::Rect host_view_bounds = host_view()->bounds();
  gfx::Rect child_view_bounds = child_view->bounds();

  TestIgnoredView* ignored_view =
      host_view()->AddChildView(std::make_unique<TestIgnoredView>());
  ignored_view->SetVisible(true);
  test::RunScheduledLayout(host_view());
  // The ignored view does not affect the layout of other views.
  EXPECT_EQ(host_view()->bounds(), host_view_bounds);
  EXPECT_EQ(child_view->bounds(), child_view_bounds);
  // The ignored view manages its own layout.
  EXPECT_EQ(ignored_view->bounds(), ignored_view->kExpectedBounds);

  // The ignored view's visibility shouldn't be changed by the layout manager.
  ignored_view->SetVisible(false);
  test::RunScheduledLayout(host_view());
  EXPECT_EQ(ignored_view->GetVisible(), false);
  EXPECT_EQ(ignored_view->bounds(), ignored_view->kExpectedBounds);
  ignored_view->SetVisible(true);
  test::RunScheduledLayout(host_view());
  EXPECT_EQ(ignored_view->GetVisible(), true);
  EXPECT_EQ(ignored_view->bounds(), ignored_view->kExpectedBounds);
}

TEST(LayoutManagerBase_ProposedLayoutTest, Equality) {
  std::array<View, 3> views;
  ProposedLayout a;
  ProposedLayout b;
  EXPECT_TRUE(a == b);
  a.host_size = {1, 2};
  EXPECT_FALSE(a == b);
  b.host_size = {1, 2};
  EXPECT_TRUE(a == b);
  a.child_layouts.push_back({&views[0], true, {1, 1, 2, 2}});
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
  a.child_layouts.push_back({&views[1], true, {1, 2, 3, 4}});
  b.child_layouts.push_back({&views[2], true, {1, 2, 3, 4}});
  EXPECT_FALSE(a == b);
  b.child_layouts[1].child_view = &views[1];
  EXPECT_TRUE(a == b);
}

class LayoutManagerBaseAvailableSizeTest : public testing::Test {
 public:
  void SetUp() override {
    view_ = std::make_unique<View>();
    layout_ =
        view_->SetLayoutManager(std::make_unique<TestLayoutManagerBase>());
  }

  View* view() { return view_.get(); }
  TestLayoutManagerBase* layout() { return layout_; }

 private:
  std::unique_ptr<View> view_;
  raw_ptr<TestLayoutManagerBase> layout_;
};

TEST_F(LayoutManagerBaseAvailableSizeTest, ReturnsCorrectValues) {
  const SizeBounds kChild1Bounds(3, 7);
  const SizeBounds kChild2Bounds(11, 13);
  View* const child1 = view()->AddChildView(std::make_unique<View>());
  View* const child2 = view()->AddChildView(std::make_unique<View>());
  View not_a_child;

  layout()->OverrideProposedLayout(
      {{10, 10},
       {{child1, true, {1, 1, 1, 1}, kChild1Bounds},
        {child2, true, {2, 2, 2, 2}, kChild2Bounds}}});
  view()->SizeToPreferredSize();

  EXPECT_EQ(kChild1Bounds, view()->GetAvailableSize(child1));
  EXPECT_EQ(kChild2Bounds, view()->GetAvailableSize(child2));
  EXPECT_EQ(SizeBounds(), view()->GetAvailableSize(&not_a_child));
}

TEST_F(LayoutManagerBaseAvailableSizeTest, AvailableSizesInNestedValuesAdd) {
  View* const child = view()->AddChildView(std::make_unique<View>());
  View* const grandchild = child->AddChildView(std::make_unique<View>());
  auto* const child_layout =
      child->SetLayoutManager(std::make_unique<TestLayoutManagerBase>());

  constexpr gfx::Size kViewSize(18, 17);
  constexpr SizeBounds kChildAvailableSize(16, 15);
  constexpr gfx::Size kChildSize(13, 12);
  constexpr SizeBounds kGrandchildAvailableSize(10, 9);
  constexpr gfx::Size kGrandchildSize(3, 2);
  layout()->OverrideProposedLayout(
      {kViewSize, {{child, true, {{3, 3}, kChildSize}, kChildAvailableSize}}});
  child_layout->OverrideProposedLayout({kChildSize,
                                        {{grandchild,
                                          true,
                                          {{2, 2}, kGrandchildSize},
                                          kGrandchildAvailableSize}}});
  view()->SizeToPreferredSize();

  EXPECT_EQ(kChildAvailableSize, view()->GetAvailableSize(child));
  SizeBounds expected;
  expected.set_width(kGrandchildAvailableSize.width() +
                     kChildAvailableSize.width() - kChildSize.width());
  expected.set_height(kGrandchildAvailableSize.height() +
                      kChildAvailableSize.height() - kChildSize.height());
  EXPECT_EQ(expected, child->GetAvailableSize(grandchild));
}

TEST_F(LayoutManagerBaseAvailableSizeTest,
       PartiallySpecifiedAvailableSizesInNestedLayoutsAddPartially) {
  View* const child = view()->AddChildView(std::make_unique<View>());
  View* const grandchild = child->AddChildView(std::make_unique<View>());
  auto* const child_layout =
      child->SetLayoutManager(std::make_unique<TestLayoutManagerBase>());

  constexpr gfx::Size kViewSize(18, 17);
  constexpr SizeBounds kChildAvailableSize(16, SizeBound());
  constexpr gfx::Size kChildSize(13, 12);
  constexpr SizeBounds kGrandchildAvailableSize(10, 9);
  constexpr gfx::Size kGrandchildSize(3, 2);
  layout()->OverrideProposedLayout(
      {kViewSize, {{child, true, {{3, 3}, kChildSize}, kChildAvailableSize}}});
  child_layout->OverrideProposedLayout({kChildSize,
                                        {{grandchild,
                                          true,
                                          {{2, 2}, kGrandchildSize},
                                          kGrandchildAvailableSize}}});
  view()->SizeToPreferredSize();

  EXPECT_EQ(kChildAvailableSize, view()->GetAvailableSize(child));
  SizeBounds expected;
  expected.set_width(kGrandchildAvailableSize.width() +
                     kChildAvailableSize.width() - kChildSize.width());
  expected.set_height(kGrandchildAvailableSize.height());
  EXPECT_EQ(expected, child->GetAvailableSize(grandchild));
}

TEST_F(LayoutManagerBaseAvailableSizeTest,
       MismatchedAvailableSizesInNestedLayoutsDoNotAdd) {
  View* const child = view()->AddChildView(std::make_unique<View>());
  View* const grandchild = child->AddChildView(std::make_unique<View>());
  auto* const child_layout =
      child->SetLayoutManager(std::make_unique<TestLayoutManagerBase>());

  constexpr gfx::Size kViewSize(18, 17);
  constexpr SizeBounds kChildAvailableSize(16, SizeBound());
  constexpr gfx::Size kChildSize(13, 12);
  constexpr SizeBounds kGrandchildAvailableSize(SizeBound(), 9);
  constexpr gfx::Size kGrandchildSize(3, 2);
  layout()->OverrideProposedLayout(
      {kViewSize, {{child, true, {{3, 3}, kChildSize}, kChildAvailableSize}}});
  child_layout->OverrideProposedLayout({kChildSize,
                                        {{grandchild,
                                          true,
                                          {{2, 2}, kGrandchildSize},
                                          kGrandchildAvailableSize}}});
  view()->SizeToPreferredSize();

  EXPECT_EQ(kChildAvailableSize, view()->GetAvailableSize(child));
  EXPECT_EQ(kGrandchildAvailableSize, child->GetAvailableSize(grandchild));
}

TEST_F(LayoutManagerBaseAvailableSizeTest,
       AvaialbleSizeChangeTriggersDescendantLayout) {
  View* const child = view()->AddChildView(std::make_unique<View>());
  TestLayoutManagerBase* const child_layout =
      child->SetLayoutManager(std::make_unique<TestLayoutManagerBase>());
  View* const grandchild = child->AddChildView(std::make_unique<View>());
  TestLayoutManagerBase* const grandchild_layout =
      grandchild->SetLayoutManager(std::make_unique<TestLayoutManagerBase>());
  View* const great_grandchild =
      grandchild->AddChildView(std::make_unique<View>());
  TestLayoutManagerBase* const great_grandchild_layout =
      great_grandchild->SetLayoutManager(
          std::make_unique<TestLayoutManagerBase>());

  // Create a default root layout with non-visible, zero-size child with no
  // available size.
  ProposedLayout root_layout;
  root_layout.child_layouts.emplace_back();
  root_layout.child_layouts[0].child_view = child;
  root_layout.child_layouts[0].available_size = SizeBounds(0, 0);

  // Set some default layouts for the rest of the hierarchy.
  layout()->OverrideProposedLayout(root_layout);
  child_layout->OverrideProposedLayout({{}, {{grandchild, false, {}, {0, 0}}}});
  grandchild_layout->OverrideProposedLayout(
      {{}, {{great_grandchild, false, {}, {0, 0}}}});

  test::RunScheduledLayout(view());

  const size_t num_grandchild_layouts = grandchild_layout->layout_count();
  const size_t num_great_grandchild_layouts =
      great_grandchild_layout->layout_count();

  // Set the same rootlayout again as a control. This should not have an effect
  // on the layout of the grand- and great-grandchild views.
  layout()->OverrideProposedLayout(root_layout);
  test::RunScheduledLayout(view());

  EXPECT_EQ(num_grandchild_layouts, grandchild_layout->layout_count());
  EXPECT_EQ(num_great_grandchild_layouts,
            great_grandchild_layout->layout_count());

  // Now set the child view to be visible with nonzero size and even larger
  // available size. Applying this layout should change the size available to
  // all views down the hierarchy, forcing a re-layout.
  root_layout.child_layouts[0].visible = true;
  root_layout.child_layouts[0].bounds = gfx::Rect(0, 0, 5, 5);
  root_layout.child_layouts[0].available_size = SizeBounds(10, 10);
  layout()->OverrideProposedLayout(root_layout);
  test::RunScheduledLayout(view());

  EXPECT_EQ(num_grandchild_layouts + 1, grandchild_layout->layout_count());
  EXPECT_EQ(num_great_grandchild_layouts + 1,
            great_grandchild_layout->layout_count());
}

using ManualLayoutUtilTest = LayoutManagerBaseManagerTest;

TEST_F(ManualLayoutUtilTest, SetCanBeVisibleForManualLayout) {
  ManualLayoutUtil manual_layout_util(layout_manager());

  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  host_view()->SizeToPreferredSize();
  test::RunScheduledLayout(host_view());

  const int old_invalidations = layout_manager()->num_invalidations();
  const int old_layouts = layout_manager()->num_layouts_generated();

  EXPECT_TRUE(child(0)->GetVisible());
  const gfx::Rect old_child1_bounds = child(0)->bounds();
  const gfx::Rect old_child2_bounds = child(1)->bounds();

  // Hide the view and remove from the layout.
  manual_layout_util.SetViewHidden(child(0), true);
  EXPECT_FALSE(child(0)->GetVisible());
  DoLayoutManagerLayout();
  EXPECT_FALSE(child(0)->GetVisible());
  // The second child should now have moved over into the space of the first.
  EXPECT_EQ(old_child1_bounds, child(1)->bounds());

  // These lines should have no effect as it is already set to hidden.
  manual_layout_util.SetViewHidden(child(0), true);
  DoLayoutManagerLayout();
  EXPECT_FALSE(child(0)->GetVisible());
  EXPECT_EQ(old_child1_bounds, child(1)->bounds());

  // Enable the view in the layout again. This won't immediately cause the view
  // to reappear; it may be made visible during the next layout pass.
  manual_layout_util.SetViewHidden(child(0), false);
  EXPECT_FALSE(child(0)->GetVisible());
  DoLayoutManagerLayout();
  EXPECT_TRUE(child(0)->GetVisible());
  // The first child should be in its original bounds.
  EXPECT_EQ(old_child1_bounds, child(0)->bounds());
  // The second child should move back to its original bounds.
  EXPECT_EQ(old_child2_bounds, child(1)->bounds());

  // Again, this should have no effect as the view is already enabled.
  manual_layout_util.SetViewHidden(child(0), false);
  DoLayoutManagerLayout();
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(old_child1_bounds, child(0)->bounds());
  EXPECT_EQ(old_child2_bounds, child(1)->bounds());

  EXPECT_EQ(old_layouts + 2, layout_manager()->num_layouts_generated());
  EXPECT_EQ(old_invalidations + 2, layout_manager()->num_invalidations());
}

TEST_F(ManualLayoutUtilTest, TemporarilyExcludeOneViewFromLayout) {
  ManualLayoutUtil manual_layout_util(layout_manager());

  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  host_view()->SizeToPreferredSize();
  test::RunScheduledLayout(host_view());

  const int old_invalidations = layout_manager()->num_invalidations();
  const int old_layouts = layout_manager()->num_layouts_generated();

  EXPECT_TRUE(child(0)->GetVisible());
  const gfx::Rect old_child1_bounds = child(0)->bounds();
  const gfx::Rect old_child2_bounds = child(1)->bounds();

  {
    auto exclusion = manual_layout_util.TemporarilyExcludeFromLayout(child(0));
    DoLayoutManagerLayout();
    // Views should have been slid over in the place of the excluded view.
    EXPECT_EQ(old_child1_bounds, child(1)->bounds());
    // Excluded view should still be visible and in its previous location.
    EXPECT_TRUE(child(0)->GetVisible());
    EXPECT_EQ(old_child1_bounds, child(0)->bounds());

    EXPECT_EQ(old_invalidations + 1, layout_manager()->num_invalidations());
    EXPECT_EQ(old_layouts + 1, layout_manager()->num_layouts_generated());
  }

  // Exiting the block should cause the layout to be restored.
  DoLayoutManagerLayout();
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(old_child1_bounds, child(0)->bounds());
  EXPECT_EQ(old_child2_bounds, child(1)->bounds());
  EXPECT_EQ(old_invalidations + 2, layout_manager()->num_invalidations());
  EXPECT_EQ(old_layouts + 2, layout_manager()->num_layouts_generated());
}

TEST_F(ManualLayoutUtilTest, TemporarilyExcludeTwoViewsFromLayout) {
  ManualLayoutUtil manual_layout_util(layout_manager());

  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  host_view()->SizeToPreferredSize();
  test::RunScheduledLayout(host_view());

  const int old_invalidations = layout_manager()->num_invalidations();
  const int old_layouts = layout_manager()->num_layouts_generated();

  EXPECT_TRUE(child(0)->GetVisible());
  const gfx::Rect old_child1_bounds = child(0)->bounds();
  const gfx::Rect old_child2_bounds = child(1)->bounds();
  const gfx::Rect old_child3_bounds = child(2)->bounds();

  {
    auto exclusion1 = manual_layout_util.TemporarilyExcludeFromLayout(child(0));
    auto exclusion2 = manual_layout_util.TemporarilyExcludeFromLayout(child(1));
    DoLayoutManagerLayout();
    // Views should have been slid over in the place of the excluded view.
    EXPECT_EQ(old_child1_bounds, child(2)->bounds());
    // Excluded view should still be visible and in its previous location.
    EXPECT_TRUE(child(0)->GetVisible());
    EXPECT_TRUE(child(1)->GetVisible());
    EXPECT_EQ(old_child1_bounds, child(0)->bounds());
    EXPECT_EQ(old_child2_bounds, child(1)->bounds());

    EXPECT_EQ(old_invalidations + 2, layout_manager()->num_invalidations());
    EXPECT_EQ(old_layouts + 1, layout_manager()->num_layouts_generated());
  }

  // Exiting the block should cause the layout to be restored.
  DoLayoutManagerLayout();
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(old_child1_bounds, child(0)->bounds());
  EXPECT_EQ(old_child2_bounds, child(1)->bounds());
  EXPECT_EQ(old_child3_bounds, child(2)->bounds());
  EXPECT_EQ(old_invalidations + 4, layout_manager()->num_invalidations());
  EXPECT_EQ(old_layouts + 2, layout_manager()->num_layouts_generated());
}

TEST_F(ManualLayoutUtilTest, ViewStaysExcludedIfAlreadyExcludedByProperty) {
  ManualLayoutUtil manual_layout_util(layout_manager());

  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  host_view()->SizeToPreferredSize();
  test::RunScheduledLayout(host_view());

  EXPECT_TRUE(child(0)->GetVisible());
  const gfx::Rect old_child1_bounds = child(0)->bounds();

  child(0)->SetProperty(kViewIgnoredByLayoutKey, true);
  test::RunScheduledLayout(host_view());

  const int old_invalidations = layout_manager()->num_invalidations();
  const int old_layouts = layout_manager()->num_layouts_generated();

  {
    // This will have no effect as the view is already excluded.
    auto exclusion = manual_layout_util.TemporarilyExcludeFromLayout(child(0));
    DoLayoutManagerLayout();
    // Views should have been slid over in the place of the excluded view.
    EXPECT_EQ(old_child1_bounds, child(1)->bounds());
    // Excluded view should still be visible and in its previous location.
    EXPECT_TRUE(child(0)->GetVisible());
    EXPECT_EQ(old_child1_bounds, child(0)->bounds());

    EXPECT_EQ(old_invalidations, layout_manager()->num_invalidations());
    EXPECT_EQ(old_layouts, layout_manager()->num_layouts_generated());
  }

  // Exiting the block should have no effect.
  DoLayoutManagerLayout();
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(old_child1_bounds, child(0)->bounds());
  EXPECT_EQ(old_child1_bounds, child(1)->bounds());
  EXPECT_EQ(old_invalidations, layout_manager()->num_invalidations());
  EXPECT_EQ(old_layouts, layout_manager()->num_layouts_generated());
}

TEST_F(ManualLayoutUtilTest, ViewHiddenDuringTemporaryExclusion) {
  ManualLayoutUtil manual_layout_util(layout_manager());

  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  host_view()->SizeToPreferredSize();
  test::RunScheduledLayout(host_view());

  EXPECT_TRUE(child(0)->GetVisible());
  const gfx::Rect old_child1_bounds = child(0)->bounds();

  const int old_invalidations = layout_manager()->num_invalidations();
  const int old_layouts = layout_manager()->num_layouts_generated();

  {
    // This will have no effect as the view is already excluded.
    auto exclusion = manual_layout_util.TemporarilyExcludeFromLayout(child(0));
    DoLayoutManagerLayout();
    // Views should have been slid over in the place of the excluded view.
    EXPECT_EQ(old_child1_bounds, child(1)->bounds());
    // Excluded view should still be visible and in its previous location.
    EXPECT_TRUE(child(0)->GetVisible());
    EXPECT_EQ(old_child1_bounds, child(0)->bounds());

    EXPECT_EQ(old_invalidations + 1, layout_manager()->num_invalidations());
    EXPECT_EQ(old_layouts + 1, layout_manager()->num_layouts_generated());

    // This changes the view to hidden, which "stacks" with exclusion. Since it
    // is a different property, it will invalidate the layout.
    manual_layout_util.SetViewHidden(child(0), true);

    DoLayoutManagerLayout();
    EXPECT_FALSE(child(0)->GetVisible());
    EXPECT_EQ(old_child1_bounds, child(1)->bounds());
    EXPECT_EQ(old_invalidations + 2, layout_manager()->num_invalidations());
    EXPECT_EQ(old_layouts + 2, layout_manager()->num_layouts_generated());
  }

  // Exiting the block will cause an invalidation, but since visibility
  // overrides inclusion, it will not change the layout.
  DoLayoutManagerLayout();
  EXPECT_FALSE(child(0)->GetVisible());
  EXPECT_EQ(old_child1_bounds, child(1)->bounds());
  EXPECT_EQ(old_invalidations + 3, layout_manager()->num_invalidations());
  EXPECT_EQ(old_layouts + 3, layout_manager()->num_layouts_generated());
}

TEST_F(ManualLayoutUtilTest, ViewUnhiddenDuringTemporaryExclusion) {
  ManualLayoutUtil manual_layout_util(layout_manager());

  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  AddChildView(kPreferredSize);
  child(0)->SetVisible(false);

  host_view()->SizeToPreferredSize();
  test::RunScheduledLayout(host_view());

  EXPECT_FALSE(child(0)->GetVisible());
  const gfx::Rect old_child2_bounds = child(1)->bounds();
  const gfx::Rect old_child3_bounds = child(2)->bounds();

  const int old_invalidations = layout_manager()->num_invalidations();
  const int old_layouts = layout_manager()->num_layouts_generated();

  {
    // This will have no effect as the view is already excluded, but will cause
    // another invalidation since exclusion and visibility are different
    // properties.
    auto exclusion = manual_layout_util.TemporarilyExcludeFromLayout(child(0));
    DoLayoutManagerLayout();
    // Views should have been slid over in the place of the hidden view.
    EXPECT_EQ(old_child2_bounds, child(1)->bounds());
    // Excluded and hidden view should not be visible.
    EXPECT_FALSE(child(0)->GetVisible());

    EXPECT_EQ(old_invalidations + 1, layout_manager()->num_invalidations());
    EXPECT_EQ(old_layouts + 1, layout_manager()->num_layouts_generated());

    // This changes the view to not hidden. While this would not affect the
    // layout due to the exclusion, but since it's a different property, it
    // still triggers an invalidation.
    manual_layout_util.SetViewHidden(child(0), false);

    // Note that since the child view is still excluded from the layout, it
    // retains its existing state, including visibility.
    DoLayoutManagerLayout();
    EXPECT_FALSE(child(0)->GetVisible());
    EXPECT_EQ(old_child2_bounds, child(1)->bounds());
    EXPECT_EQ(old_invalidations + 2, layout_manager()->num_invalidations());
    EXPECT_EQ(old_layouts + 2, layout_manager()->num_layouts_generated());
  }

  // Exiting the block will re-add the view, which is now made visible by the
  // layout.
  DoLayoutManagerLayout();
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(old_child2_bounds, child(0)->bounds());
  EXPECT_EQ(old_child3_bounds, child(1)->bounds());
  EXPECT_EQ(old_invalidations + 3, layout_manager()->num_invalidations());
  EXPECT_EQ(old_layouts + 3, layout_manager()->num_layouts_generated());
}

}  // namespace views

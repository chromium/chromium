// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include "base/test/gtest_util.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/test/views_test_base.h"

namespace views::test {

namespace {
class TestLazyLoadingView : public View {
  METADATA_HEADER(TestLazyLoadingView, View)

 public:
  TestLazyLoadingView() = default;
  TestLazyLoadingView(const TestLazyLoadingView&) = delete;
  TestLazyLoadingView& operator=(const TestLazyLoadingView&) = delete;
  ~TestLazyLoadingView() override = default;

  // View:
  void OnAccessibilityInitializing(ui::AXNodeData* data) override {
    views::ViewAccessibilityUtils::Merge(/*source*/ lazy_loading_data_,
                                         /*destination*/ *data);
  }

  ui::AXNodeData lazy_loading_data_;
};

BEGIN_METADATA(TestLazyLoadingView)
END_METADATA

}  // namespace

class ViewAccessibilityTest : public ViewsTestBase {
 public:
  ViewAccessibilityTest() : ax_mode_setter_(ui::kAXModeComplete) {}
  ViewAccessibilityTest(const ViewAccessibilityTest&) = delete;
  ViewAccessibilityTest& operator=(const ViewAccessibilityTest&) = delete;
  ~ViewAccessibilityTest() override = default;

  void TearDown() override { ViewsTestBase::TearDown(); }

 protected:
  // std::unique_ptr<Widget> widget_;
  ::ui::ScopedAXModeSetter ax_mode_setter_;
};

TEST_F(ViewAccessibilityTest, LazyLoadingNoOverlap) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  EXPECT_FALSE(lazy_loading_view->GetViewAccessibility().is_initialized());

  lazy_loading_view->lazy_loading_data_.AddStringAttribute(
      ax::mojom::StringAttribute::kName, "My button");
  lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization();

  ui::AXNodeData data;
  lazy_loading_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "My button");
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapString) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddStringAttribute(
      ax::mojom::StringAttribute::kName, "Lazy Name");
  lazy_loading_view->GetViewAccessibility().SetName("My button");
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapBool) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddBoolAttribute(
      ax::mojom::BoolAttribute::kSelected, true);
  lazy_loading_view->GetViewAccessibility().SetIsSelected(true);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapInt) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddIntAttribute(
      ax::mojom::IntAttribute::kPosInSet, 1);
  lazy_loading_view->GetViewAccessibility().SetPosInSet(2);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

// Need to rebase to use this function.
TEST_F(ViewAccessibilityTest, LazyLoadingOverlapFloat) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddFloatAttribute(
      ax::mojom::FloatAttribute::kChildTreeScale, 1.0f);
  lazy_loading_view->GetViewAccessibility().SetChildTreeID(
      ui::AXTreeID::CreateNewAXTreeID());
  lazy_loading_view->GetViewAccessibility().SetChildTreeScaleFactor(2.0f);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, LazyLoadingOverlapIntList) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  std::vector<int32_t> list_1 = {1, 2, 3};
  lazy_loading_view->lazy_loading_data_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, list_1);
  std::vector<int32_t> list_2 = {4, 5, 6};
  lazy_loading_view->GetViewAccessibility().SetCharacterOffsets(list_2);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, CantSetStateInLazyLoading) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddState(ax::mojom::State::kCollapsed);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, CantSetActionsInLazyLoading) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  lazy_loading_view->lazy_loading_data_.AddAction(
      ax::mojom::Action::kDoDefault);
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

TEST_F(ViewAccessibilityTest, CantSetRelativeBoundsInLazyLoading) {
  TestLazyLoadingView* lazy_loading_view = new TestLazyLoadingView();
  lazy_loading_view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  gfx::RectF relative_bounds(0, 0, 10, 10);
  lazy_loading_view->lazy_loading_data_.relative_bounds.bounds =
      relative_bounds;
  EXPECT_DCHECK_DEATH(
      lazy_loading_view->GetViewAccessibility().CompleteCacheInitialization());
}

}  // namespace views::test

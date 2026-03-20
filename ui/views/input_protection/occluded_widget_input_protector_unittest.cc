// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class OccludedWidgetInputProtectorTestBase : public WidgetTest {
 public:
  OccludedWidgetInputProtectorTestBase() = default;

  const std::set<Widget*>& always_on_top_widgets() {
    return OccludedWidgetInputProtector::GetInstance()
        ->always_on_top_widgets_for_testing();
  }

  bool IsObserving(Widget* widget) {
    return widget->HasObserver(OccludedWidgetInputProtector::GetInstance());
  }

 protected:
  std::unique_ptr<Widget> CreateWidgetWithZOrder(
      ui::ZOrderLevel z_order = ui::ZOrderLevel::kNormal) {
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.z_order = z_order;
    params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
    auto widget = std::make_unique<Widget>();
    widget->Init(std::move(params));
    return widget;
  }
};

class OccludedWidgetInputProtectorTest
    : public OccludedWidgetInputProtectorTestBase {
 public:
  OccludedWidgetInputProtectorTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kEnableInputProtection);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OccludedWidgetInputProtectorTest, TracksAlwaysOnTopWidget) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  EXPECT_TRUE(IsObserving(widget.get()));

  // Not tracked yet because it is not visible.
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, DoesNotTrackNormalWidget) {
  auto widget = CreateWidgetWithZOrder();
  EXPECT_FALSE(IsObserving(widget.get()));

  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, CleanupOnDestroy) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget.reset();
  EXPECT_TRUE(always_on_top_widgets().empty());
}

TEST_F(OccludedWidgetInputProtectorTest, HandlesZOrderLevelChanges) {
  // Start with a normal widget.
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kNormal);
  EXPECT_FALSE(IsObserving(widget.get()));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  // Change Z-order to always-on-top.
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);

  // It should now be observed and tracked (since it is visible).
  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));

  // Change back to normal.
  widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  EXPECT_FALSE(IsObserving(widget.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, HandlesZOrderLevelChangesWhileHidden) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kNormal);
  EXPECT_FALSE(IsObserving(widget.get()));

  // Change to AOT while hidden.
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  // Should be observed now, but not in the visible set.
  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  // Showing should add it to the set.
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));

  // Hiding it.
  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();
  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  // Changing to normal while hidden.
  widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  EXPECT_FALSE(IsObserving(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, TracksHigherZOrderLevels) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kSecuritySurface);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, TracksMultipleWidgets) {
  auto widget1 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  auto widget2 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);

  EXPECT_TRUE(IsObserving(widget1.get()));
  EXPECT_TRUE(IsObserving(widget2.get()));

  EXPECT_FALSE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget2.get()));

  widget1->Show();
  WidgetVisibleWaiter(widget1.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_TRUE(IsObserving(widget1.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget2.get()));

  widget2->Show();
  WidgetVisibleWaiter(widget2.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget2.get()));
  EXPECT_TRUE(IsObserving(widget2.get()));

  widget1->Hide();
  WidgetVisibleWaiter(widget1.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_TRUE(IsObserving(widget1.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget2.get()));

  widget2->Hide();
  WidgetVisibleWaiter(widget2.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget2.get()));
  EXPECT_TRUE(IsObserving(widget2.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, HandlesDestroyWhileHidden) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget.reset();
  EXPECT_TRUE(always_on_top_widgets().empty());
}

class OccludedWidgetInputProtectorDisabledTest
    : public OccludedWidgetInputProtectorTestBase {
 public:
  OccludedWidgetInputProtectorDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kEnableInputProtection);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OccludedWidgetInputProtectorDisabledTest, DoesNotTrackWhenDisabled) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  EXPECT_FALSE(IsObserving(widget.get()));
  widget->Show();

  EXPECT_TRUE(always_on_top_widgets().empty());
}

}  // namespace views::test

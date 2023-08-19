// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/sublevel_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace views {

enum WidgetShowType { kShowActive, kShowInactive };

class SublevelManagerTest : public ViewsTestBase,
                            public testing::WithParamInterface<
                                std::tuple<ViewsTestBase::NativeWidgetType,
                                           WidgetShowType,
                                           Widget::InitParams::Activatable>> {
 public:
  SublevelManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWidgetLayering);
  }

  void SetUp() override {
    set_native_widget_type(
        std::get<ViewsTestBase::NativeWidgetType>(GetParam()));
    ViewsTestBase::SetUp();
  }

  std::unique_ptr<Widget> CreateChildWidget(
      Widget* parent,
      ui::ZOrderLevel level,
      int sublevel,
      Widget::InitParams::Activatable activatable) {
    Widget::InitParams params = CreateParamsForTestWidget();
    params.z_order = level;
    params.sublevel = sublevel;
    params.activatable = activatable;
    params.parent = parent->GetNativeView();
    return CreateTestWidget(std::move(params));
  }

  // Call Show() or ShowInactive() depending on WidgetShowType.
  void ShowWidget(const std::unique_ptr<Widget>& widget) {
    WidgetShowType show_type = std::get<WidgetShowType>(GetParam());
    if (show_type == WidgetShowType::kShowActive)
      widget->Show();
    else
      widget->ShowInactive();
    test::WidgetVisibleWaiter(widget.get()).Wait();
  }

  static std::string PrintTestName(
      const ::testing::TestParamInfo<SublevelManagerTest::ParamType>& info) {
    std::string test_name;
    switch (std::get<ViewsTestBase::NativeWidgetType>(info.param)) {
      case ViewsTestBase::NativeWidgetType::kDefault:
        test_name += "DefaultWidget";
        break;
      case ViewsTestBase::NativeWidgetType::kDesktop:
        test_name += "DesktopWidget";
        break;
    }
    test_name += "_";
    switch (std::get<WidgetShowType>(info.param)) {
      case WidgetShowType::kShowActive:
        test_name += "ShowActive";
        break;
      case WidgetShowType::kShowInactive:
        test_name += "ShowInactive";
        break;
    }
    test_name += "_";
    switch (std::get<Widget::InitParams::Activatable>(info.param)) {
      case Widget::InitParams::Activatable::kNo:
        test_name += "NotActivatable";
        break;
      case Widget::InitParams::Activatable::kYes:
        test_name += "Activatable";
        break;
      default:
        NOTREACHED_NORETURN();
    }
    return test_name;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Widgets should be stacked according to their sublevel regardless
// the order of showing.
TEST_P(SublevelManagerTest, EnsureSublevel) {
  std::unique_ptr<Widget> root = CreateTestWidget();
  std::unique_ptr<Widget> children[3];

  for (int sublevel = 0; sublevel < 3; sublevel++) {
    children[sublevel] = CreateChildWidget(
        root.get(), ui::ZOrderLevel::kNormal, sublevel,
        std::get<Widget::InitParams::Activatable>(GetParam()));
  }

  ShowWidget(root);

  int order[] = {0, 1, 2};
  do {
    for (int i : order)
      ShowWidget(children[i]);
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) {
        if (i < j) {
          EXPECT_FALSE(test::WidgetTest::IsWindowStackedAbove(
              children[i].get(), children[j].get()));
        } else if (i > j) {
          EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(
              children[i].get(), children[j].get()));
        }
      }
  } while (std::next_permutation(order, order + 3));
}

// Level should takes precedence over sublevel.
// TODO(crbug.com/1358586): disabled because currently non-desktop widgets
// ignore z-order level (except on ash) and we don't have a reliable way to
// test desktop widgets.
TEST_P(SublevelManagerTest, DISABLED_LevelSupersedeSublevel) {
  std::unique_ptr<Widget> root = CreateTestWidget();
  std::unique_ptr<Widget> low_level_widget, high_level_widget;

  // `high_level_widget` should be above `low_level_widget` that has a lower
  // level and a higher sublevel.
  low_level_widget =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kNormal, 1,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  high_level_widget =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kFloatingWindow, 0,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  ShowWidget(root);
  ShowWidget(high_level_widget);
  ShowWidget(low_level_widget);

  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(high_level_widget.get(),
                                                     low_level_widget.get()));
}

// Widgets are re-ordered only within the same level.
TEST_P(SublevelManagerTest, SublevelOnlyEnsuredWithinSameLevel) {
  std::unique_ptr<Widget> root = CreateTestWidget();
  std::unique_ptr<Widget> low_level_widget1, low_level_widget2,
      high_level_widget;

  low_level_widget1 =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kNormal, 1,
                        std::get<Widget::InitParams::Activatable>(GetParam()));
  low_level_widget2 =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kNormal, 2,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  high_level_widget =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kFloatingWindow, 0,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  ShowWidget(root);
  ShowWidget(low_level_widget2);
  ShowWidget(low_level_widget1);
  ShowWidget(high_level_widget);

  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(high_level_widget.get(),
                                                     low_level_widget1.get()));
  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(high_level_widget.get(),
                                                     low_level_widget2.get()));
  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(low_level_widget2.get(),
                                                     low_level_widget1.get()));
}

// SetSublevel() should trigger re-ordering.
TEST_P(SublevelManagerTest, SetSublevel) {
  std::unique_ptr<Widget> root = CreateTestWidget();
  std::unique_ptr<Widget> child1, child2;

  child1 =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kNormal, 1,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  child2 =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kNormal, 2,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  ShowWidget(root);
  ShowWidget(child2);
  ShowWidget(child1);
  EXPECT_TRUE(
      test::WidgetTest::IsWindowStackedAbove(child2.get(), child1.get()));

  child1->SetZOrderSublevel(3);
  EXPECT_TRUE(
      test::WidgetTest::IsWindowStackedAbove(child1.get(), child2.get()));
}

TEST_P(SublevelManagerTest, GetSublevel) {
  std::unique_ptr<Widget> root = CreateTestWidget();
  std::unique_ptr<Widget> child1, child2;

  child1 =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kNormal, 1,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  child2 =
      CreateChildWidget(root.get(), ui::ZOrderLevel::kNormal, 2,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  EXPECT_EQ(child1->GetZOrderSublevel(), 1);
  EXPECT_EQ(child2->GetZOrderSublevel(), 2);
}

// The stacking order between non-sibling widgets depend on the sublevels
// of the children of their most recent common ancestor.
TEST_P(SublevelManagerTest, GrandChildren) {
  std::unique_ptr<Widget> root = CreateTestWidget();
  std::unique_ptr<Widget> children[2];
  std::unique_ptr<Widget> grand_children[2][2];

  for (int i = 0; i < 2; i++) {
    children[i] = CreateChildWidget(
        root.get(), ui::ZOrderLevel::kNormal, i,
        std::get<Widget::InitParams::Activatable>(GetParam()));
    for (int j = 0; j < 2; j++) {
      grand_children[i][j] = CreateChildWidget(
          children[i].get(), ui::ZOrderLevel::kNormal, j,
          std::get<Widget::InitParams::Activatable>(GetParam()));
    }
  }

  ShowWidget(root);
  ShowWidget(children[1]);
  ShowWidget(children[0]);
  ShowWidget(grand_children[1][0]);
  ShowWidget(grand_children[0][1]);

  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(children[1].get(),
                                                     children[0].get()));

  // Even though grand_children[0][1] is shown later, because its parent has a
  // lower sublevel than grand_children[1][0]'s parent, it should be behind.
  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(
      grand_children[1][0].get(), grand_children[0][1].get()));
}

// The sublevel manager should be able to handle the Widget re-parenting.
TEST_P(SublevelManagerTest, WidgetReparent) {
  std::unique_ptr<Widget> root1 = CreateTestWidget();
  std::unique_ptr<Widget> root2 = CreateTestWidget();
  std::unique_ptr<Widget> child;

  child =
      CreateChildWidget(root1.get(), ui::ZOrderLevel::kNormal, 1,
                        std::get<Widget::InitParams::Activatable>(GetParam()));

  ShowWidget(root1);
  ShowWidget(child);

  ShowWidget(root2);
  Widget::ReparentNativeView(child->GetNativeView(), root2->GetNativeView());
  ShowWidget(child);

#if !BUILDFLAG(IS_MAC)
  // Mac does not allow re-parenting child widgets to nullptr.
  Widget::ReparentNativeView(child->GetNativeView(), nullptr);
  ShowWidget(child);
#endif
}

// Invisible widgets should be skipped to work around MacOS where
// stacking above them is no-op (crbug.com/1369180).
// When they become invisible, sublevels should be respected.
TEST_P(SublevelManagerTest, SkipInvisibleWidget) {
  std::unique_ptr<Widget> root = CreateTestWidget();
  std::unique_ptr<Widget> children[3];

  ShowWidget(root);
  for (int i = 0; i < 3; i++) {
    children[i] = CreateChildWidget(
        root.get(), ui::ZOrderLevel::kNormal, i,
        std::get<Widget::InitParams::Activatable>(GetParam()));
    ShowWidget(children[i]);

    // Hide the second widget.
    if (i == 1)
      children[i]->Hide();
  }

  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(children[2].get(),
                                                     children[0].get()));

  ShowWidget(children[1]);
  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(children[1].get(),
                                                     children[0].get()));
  EXPECT_TRUE(test::WidgetTest::IsWindowStackedAbove(children[2].get(),
                                                     children[1].get()));
}

// TODO(crbug.com/1333445): We should also test NativeWidgetType::kDesktop,
// but currently IsWindowStackedAbove() does not work for desktop widgets.
INSTANTIATE_TEST_SUITE_P(
    ,
    SublevelManagerTest,
    ::testing::Combine(
        ::testing::Values(ViewsTestBase::NativeWidgetType::kDefault),
        ::testing::Values(WidgetShowType::kShowActive,
                          WidgetShowType::kShowInactive),
        ::testing::Values(Widget::InitParams::Activatable::kNo,
                          Widget::InitParams::Activatable::kYes)),
    SublevelManagerTest::PrintTestName);

}  // namespace views

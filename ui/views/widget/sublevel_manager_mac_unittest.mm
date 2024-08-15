// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/sublevel_manager.h"

#include <AppKit/AppKit.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/mac/mac_util.h"
#include "build/buildflag.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace views {

enum WidgetShowType { kShowActive, kShowInactive };

class SublevelManagerMacTest
    : public ViewsTestBase,
      public testing::WithParamInterface<
          std::tuple<WidgetShowType, Widget::InitParams::Activatable>> {
 public:
  SublevelManagerMacTest() = default;

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
      const ::testing::TestParamInfo<SublevelManagerMacTest::ParamType>& info) {
    std::string test_name;
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
        NOTREACHED();
    }
    return test_name;
  }
};

// Disabled widgets are ignored when its siblings are re-ordered.
TEST_P(SublevelManagerMacTest, ExplicitUntrack) {
  std::unique_ptr<Widget> root =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  std::unique_ptr<Widget> root2 =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  std::array<std::unique_ptr<Widget>, 3> children;

  ShowWidget(root);
  ShowWidget(root2);
  for (size_t i = 0; i < children.size(); i++) {
    children[i] = CreateChildWidget(
        root.get(), ui::ZOrderLevel::kNormal, i,
        std::get<Widget::InitParams::Activatable>(GetParam()));
    ShowWidget(children[i]);

    // Disable the second widget.
    if (i == 1) {
      children[i]->parent()->GetSublevelManager()->UntrackChildWidget(
          children[i].get());
    }
  }

  NSWindow* root_nswindow = root->GetNativeWindow().GetNativeNSWindow();
  NSWindow* root2_nswindow = root2->GetNativeWindow().GetNativeNSWindow();
  NSWindow* child2_nswindow =
      children[1]->GetNativeWindow().GetNativeNSWindow();

  // Reparent `child2` to root2 at the NSWindow level but not at the Widget
  // level.
  [root_nswindow removeChildWindow:child2_nswindow];
  [root2_nswindow addChildWindow:child2_nswindow ordered:NSWindowAbove];

  children[1]->GetSublevelManager()->EnsureOwnerSublevel();

  // The parent of child2 does not change.
  EXPECT_EQ([child2_nswindow parentWindow], root2_nswindow);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SublevelManagerMacTest,
    ::testing::Combine(
        ::testing::Values(WidgetShowType::kShowActive,
                          WidgetShowType::kShowInactive),
        ::testing::Values(Widget::InitParams::Activatable::kNo,
                          Widget::InitParams::Activatable::kYes)),
    SublevelManagerMacTest::PrintTestName);

}  // namespace views

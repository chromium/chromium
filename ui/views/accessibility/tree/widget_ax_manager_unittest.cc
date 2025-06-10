// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class WidgetAXManagerTest : public test::WidgetTest {
 protected:
  WidgetAXManagerTest() = default;
  ~WidgetAXManagerTest() override = default;

  void SetUp() override {
    WidgetTest::SetUp();
    widget_.reset(CreateTopLevelPlatformWidget());
  }

  void TearDown() override {
    widget_.reset();
    WidgetTest::TearDown();
  }

  Widget* widget() { return widget_.get(); }
  WidgetAXManager* manager() { return widget_->ax_manager(); }

 private:
  WidgetAutoclosePtr widget_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityTreeForViews};
};

TEST_F(WidgetAXManagerTest, InitiallyDisabled) {
  EXPECT_FALSE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, EnableSetsEnabled) {
  manager()->Enable();
  EXPECT_TRUE(manager()->is_enabled());
}

TEST_F(WidgetAXManagerTest, IsEnabledAfterAXModeAdded) {
  // Initially, the manager should not be enabled.
  ASSERT_FALSE(manager()->is_enabled());

  // Simulate that AXMode with kNativeAPIs was added.
  ui::AXPlatform::GetInstance().NotifyModeAdded(ui::AXMode::kNativeAPIs);
  EXPECT_TRUE(manager()->is_enabled());
}

class WidgetAXManagerOffTest : public ViewsTestBase {
 protected:
  WidgetAXManagerOffTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kAccessibilityTreeForViews);
  }
  ~WidgetAXManagerOffTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This death test verifies that Create() crashes (via CHECK) when the flag is
// off.
TEST_F(WidgetAXManagerOffTest, CrashesWhenFlagOff) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 500, 500);
  widget->Init(std::move(params));

  auto create_manager = [&]() { WidgetAXManager manager(widget.get()); };
  EXPECT_DEATH(create_manager(), "");

  widget->CloseNow();
  widget.reset();
}

}  // namespace views::test

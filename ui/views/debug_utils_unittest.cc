// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/debug_utils.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/dcheck_is_on.h"
#include "ui/compositor/layer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

using DebugUtilsTest = ViewsTestBase;

TEST_F(DebugUtilsTest, PrintWindowHierarchy) {
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.name = "TestWidget";
  auto widget = std::make_unique<Widget>();
  widget->Init(std::move(params));

  std::ostringstream out;
  PrintWindowHierarchy(widget->GetNativeWindow(), &out);
  std::string output = out.str();

  // Verify that the output contains the expected sections.
#if defined(USE_AURA)
  EXPECT_NE(output.find("--- Window Hierarchy ---"), std::string::npos);
#if DCHECK_IS_ON()
  EXPECT_EQ(output.find("Window hierarchy is only available in DCHECK builds."),
            std::string::npos);
#else
  EXPECT_NE(output.find("Window hierarchy is only available in DCHECK builds."),
            std::string::npos);
#endif
#endif
  EXPECT_NE(output.find("--- Widget Information ---"), std::string::npos);
  EXPECT_EQ(output.find("--- ui::Layer Tree ---"), std::string::npos);

  // Verify that the output contains specific details.
  EXPECT_NE(output.find("name=TestWidget"), std::string::npos);
}

TEST_F(DebugUtilsTest, PrintLayerHierarchy) {
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.name = "TestWidget";
  auto widget = std::make_unique<Widget>();
  widget->Init(std::move(params));

  // Add a layer-backed view to verify it appears in the layer tree.
  View* root_view = widget->GetRootView();
  View* child_view = root_view->AddChildView(std::make_unique<View>());
  child_view->SetPaintToLayer();
  child_view->layer()->SetName("TestChildLayer");

  std::ostringstream out;
  PrintLayerHierarchy(widget->GetNativeWindow(), &out);
  std::string output = out.str();

  // Verify that the output contains the expected sections.
  EXPECT_NE(output.find("--- ui::Layer Tree ---"), std::string::npos);
  EXPECT_EQ(output.find("--- Window Hierarchy ---"), std::string::npos);
  EXPECT_EQ(output.find("--- Widget Information ---"), std::string::npos);

  // Verify that the custom layer is present in the output.
  EXPECT_NE(output.find("TestChildLayer"), std::string::npos);
}

}  // namespace views

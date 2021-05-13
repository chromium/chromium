// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

class InstallableInkDropTest : public ViewsTestBase {
 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();

    root_view_ = std::make_unique<View>();
    // Ink drop layers get installed as siblings to their host view's
    // layer. Hence, there needs to be a root view with a layer above them.
    root_view_->SetPaintToLayer();
  }

  View* root_view() { return root_view_.get(); }
  std::unique_ptr<View> own_root_view() { return std::move(root_view_); }

 private:
  std::unique_ptr<View> root_view_;
};

TEST_F(InstallableInkDropTest, LayerIsAddedAndRemoved) {
  View* view = root_view()->AddChildView(std::make_unique<View>());
  view->SetPaintToLayer();
  EXPECT_EQ(1, static_cast<int>(root_view()->layer()->children().size()));

  {
    InstallableInkDrop ink_drop(view);
    EXPECT_EQ(2, static_cast<int>(root_view()->layer()->children().size()));
  }

  EXPECT_EQ(1, static_cast<int>(root_view()->layer()->children().size()));
}

TEST_F(InstallableInkDropTest, LayerSizeTracksViewSize) {
  View* view = root_view()->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(gfx::Rect(0, 0, 10, 10));

  InstallableInkDrop ink_drop(view);
  EXPECT_EQ(view->size(), ink_drop.layer_for_testing()->size());

  view->SetBoundsRect(gfx::Rect(0, 0, 20, 15));
  EXPECT_EQ(view->size(), ink_drop.layer_for_testing()->size());

  view->SetBoundsRect(gfx::Rect(10, 10, 30, 30));
  EXPECT_EQ(view->size(), ink_drop.layer_for_testing()->size());
}

TEST_F(InstallableInkDropTest, UpdatesState) {
  View* view = root_view()->AddChildView(std::make_unique<View>());
  InstallableInkDrop ink_drop(view);

  // Initial state should be HIDDEN.
  EXPECT_EQ(ink_drop.GetTargetInkDropState(), InkDropState::HIDDEN);

  ink_drop.AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(ink_drop.GetTargetInkDropState(), InkDropState::ACTIVATED);

  ink_drop.SnapToHidden();
  EXPECT_EQ(ink_drop.GetTargetInkDropState(), InkDropState::HIDDEN);

  ink_drop.SnapToActivated();
  EXPECT_EQ(ink_drop.GetTargetInkDropState(), InkDropState::ACTIVATED);
}

TEST_F(InstallableInkDropTest, HighlightStates) {
  View* view = root_view()->AddChildView(std::make_unique<View>());
  InstallableInkDrop ink_drop(view);

  // Initial state should be false.
  EXPECT_FALSE(ink_drop.IsHighlightFadingInOrVisible());

  ink_drop.SetFocused(true);
  EXPECT_TRUE(ink_drop.IsHighlightFadingInOrVisible());

  ink_drop.SetFocused(false);
  EXPECT_FALSE(ink_drop.IsHighlightFadingInOrVisible());

  ink_drop.SetHovered(true);
  EXPECT_TRUE(ink_drop.IsHighlightFadingInOrVisible());

  ink_drop.SetHovered(false);
  EXPECT_FALSE(ink_drop.IsHighlightFadingInOrVisible());
}

TEST_F(InstallableInkDropTest, Paint) {
  std::unique_ptr<Widget> widget = CreateTestWidget();
  View* root_view = widget->SetContentsView(own_root_view());
  View* view = root_view->AddChildView(std::make_unique<View>());
  InstallableInkDrop ink_drop(view);

  views::InstallableInkDropConfig config;
  config.base_color = SK_ColorCYAN;
  config.ripple_opacity = 0.05;
  config.highlight_opacity = 0.07;
  ink_drop.SetConfig(config);
  ink_drop.AnimateToState(InkDropState::ACTIVATED);

  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  ink_drop.OnPaintLayer(
      ui::PaintContext(list.get(), 1.f, view->bounds(), false));
  EXPECT_GT(2u, list->num_paint_ops());
}

}  // namespace views

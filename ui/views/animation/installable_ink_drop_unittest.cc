// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

class InstallableInkDropTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Ink drop layers get installed as siblings to their host view's
    // layer. Hence, there needs to be a root view with a layer above them.
    root_view_.SetPaintToLayer();
  }

  View* root_view() { return &root_view_; }

 private:
  base::test::TaskEnvironment task_environment_;

  View root_view_;
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
}

}  // namespace views

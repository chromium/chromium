// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_host.h"

#include <memory>

#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_highlight_test_api.h"
#include "ui/views/animation/test/square_ink_drop_ripple_test_api.h"

namespace views {

namespace {

// Test specific subclass of InkDropRipple that returns a test api from
// GetTestApi().
class TestInkDropRipple : public SquareInkDropRipple {
 public:
  TestInkDropRipple(const gfx::Size& large_size,
                    int large_corner_radius,
                    const gfx::Size& small_size,
                    int small_corner_radius,
                    const gfx::Point& center_point,
                    SkColor color,
                    float visible_opacity)
      : SquareInkDropRipple(large_size,
                            large_corner_radius,
                            small_size,
                            small_corner_radius,
                            center_point,
                            color,
                            visible_opacity) {}

  ~TestInkDropRipple() override = default;

  test::InkDropRippleTestApi* GetTestApi() override {
    if (!test_api_)
      test_api_ = std::make_unique<test::SquareInkDropRippleTestApi>(this);
    return test_api_.get();
  }

 private:
  std::unique_ptr<test::InkDropRippleTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropRipple);
};

// Test specific subclass of InkDropHighlight that returns a test api from
// GetTestApi().
class TestInkDropHighlight : public InkDropHighlight {
 public:
  TestInkDropHighlight(const gfx::Size& size,
                       int corner_radius,
                       const gfx::PointF& center_point,
                       SkColor color)
      : InkDropHighlight(size, corner_radius, center_point, color) {}

  ~TestInkDropHighlight() override = default;

  test::InkDropHighlightTestApi* GetTestApi() override {
    if (!test_api_)
      test_api_ = std::make_unique<test::InkDropHighlightTestApi>(this);
    return test_api_.get();
  }

 private:
  std::unique_ptr<test::InkDropHighlightTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropHighlight);
};

}  // namespace

TestInkDropHost::TestInkDropHost() = default;

TestInkDropHost::~TestInkDropHost() = default;

void TestInkDropHost::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  ++num_ink_drop_layers_added_;
}

void TestInkDropHost::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  ++num_ink_drop_layers_removed_;
}

std::unique_ptr<InkDropRipple> TestInkDropHost::CreateInkDropRipple() const {
  std::unique_ptr<InkDropRipple> ripple(new TestInkDropRipple(
      size(), 0, size(), 0, gfx::Point(), SK_ColorBLACK, 0.175f));
  if (disable_timers_for_test_)
    ripple->GetTestApi()->SetDisableAnimationTimers(true);
  num_ink_drop_ripples_created_++;
  last_ink_drop_ripple_ = ripple.get();
  return ripple;
}

std::unique_ptr<InkDropHighlight> TestInkDropHost::CreateInkDropHighlight()
    const {
  std::unique_ptr<InkDropHighlight> highlight;
  highlight = std::make_unique<TestInkDropHighlight>(size(), 0, gfx::PointF(),
                                                     SK_ColorBLACK);
  if (disable_timers_for_test_)
    highlight->GetTestApi()->SetDisableAnimationTimers(true);
  num_ink_drop_highlights_created_++;
  last_ink_drop_highlight_ = highlight.get();
  return highlight;
}

}  // namespace views

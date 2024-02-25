// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_host.h"

#include <memory>

#include "base/functional/bind.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_highlight_test_api.h"
#include "ui/views/animation/test/square_ink_drop_ripple_test_api.h"

namespace views {
class InkDropHost;

namespace {

// Test specific subclass of InkDropRipple that returns a test api from
// GetTestApi().
class TestInkDropRipple : public SquareInkDropRipple {
 public:
  TestInkDropRipple(InkDropHost* ink_drop_host,
                    const gfx::Size& large_size,
                    int large_corner_radius,
                    const gfx::Size& small_size,
                    int small_corner_radius,
                    const gfx::Point& center_point,
                    SkColor color,
                    float visible_opacity)
      : SquareInkDropRipple(ink_drop_host,
                            large_size,
                            large_corner_radius,
                            small_size,
                            small_corner_radius,
                            center_point,
                            color,
                            visible_opacity) {}

  TestInkDropRipple(const TestInkDropRipple&) = delete;
  TestInkDropRipple& operator=(const TestInkDropRipple&) = delete;

  ~TestInkDropRipple() override = default;

  test::InkDropRippleTestApi* GetTestApi() override {
    if (!test_api_)
      test_api_ = std::make_unique<test::SquareInkDropRippleTestApi>(this);
    return test_api_.get();
  }

 private:
  std::unique_ptr<test::InkDropRippleTestApi> test_api_;
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

  TestInkDropHighlight(const TestInkDropHighlight&) = delete;
  TestInkDropHighlight& operator=(const TestInkDropHighlight&) = delete;

  ~TestInkDropHighlight() override = default;

  test::InkDropHighlightTestApi* GetTestApi() override {
    if (!test_api_)
      test_api_ = std::make_unique<test::InkDropHighlightTestApi>(this);
    return test_api_.get();
  }

 private:
  std::unique_ptr<test::InkDropHighlightTestApi> test_api_;
};

}  // namespace

TestInkDropHost::TestInkDropHost(
    InkDropImpl::AutoHighlightMode auto_highlight_mode) {
  InkDrop::Install(this, std::make_unique<InkDropHost>(this));
  InkDrop::Get(this)->SetCreateInkDropCallback(base::BindRepeating(
      [](TestInkDropHost* host,
         InkDropImpl::AutoHighlightMode auto_highlight_mode)
          -> std::unique_ptr<views::InkDrop> {
        return std::make_unique<views::InkDropImpl>(
            InkDrop::Get(host), host->size(), auto_highlight_mode);
      },
      this, auto_highlight_mode));

  InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
      [](TestInkDropHost* host) -> std::unique_ptr<views::InkDropHighlight> {
        auto highlight = std::make_unique<TestInkDropHighlight>(
            host->size(), 0, gfx::PointF(), SK_ColorBLACK);
        if (host->disable_timers_for_test_)
          highlight->GetTestApi()->SetDisableAnimationTimers(true);
        host->num_ink_drop_highlights_created_++;
        return highlight;
      },
      this));
  InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](TestInkDropHost* host) -> std::unique_ptr<views::InkDropRipple> {
        auto ripple = std::make_unique<TestInkDropRipple>(
            InkDrop::Get(host), host->size(), 0, host->size(), 0, gfx::Point(),
            SK_ColorBLACK, 0.175f);
        if (host->disable_timers_for_test_)
          ripple->GetTestApi()->SetDisableAnimationTimers(true);
        host->num_ink_drop_ripples_created_++;
        return ripple;
      },
      this));
}

void TestInkDropHost::AddLayerToRegion(ui::Layer* layer,
                                       views::LayerRegion region) {
  ++num_ink_drop_layers_added_;
}

void TestInkDropHost::RemoveLayerFromRegions(ui::Layer* layer) {
  ++num_ink_drop_layers_removed_;
}

TestInkDropHost::~TestInkDropHost() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

}  // namespace views

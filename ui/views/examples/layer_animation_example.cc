// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/layer_animation_example.h"

#include <memory>

#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

class RoundedRectLayerPainter : public ui::LayerDelegate {
 public:
  RoundedRectLayerPainter(View* container) : container_(container) {}  // NOLINT
  ~RoundedRectLayerPainter() override = default;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

 private:
  View* container_;
};

void RoundedRectLayerPainter::OnPaintLayer(const ui::PaintContext& context) {
  const SkColor colors[2] = {gfx::kGoogleBlue700, gfx::kGoogleBlue300};
  cc::PaintFlags flags;
  gfx::Rect local_bounds = gfx::Rect(container_->layer()->size());
  ui::PaintRecorder recorder(context, local_bounds.size());
  gfx::Canvas* canvas = recorder.canvas();
  const float dsf = canvas->UndoDeviceScaleFactor();
  gfx::RectF local_bounds_f = gfx::RectF(local_bounds);
  local_bounds_f.Scale(dsf);
  SkRect bounds = gfx::RectToSkRect(gfx::ToEnclosingRect(local_bounds_f));
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeRadialGradient(
      SkPoint::Make(bounds.centerX(), bounds.centerY()), bounds.width() / 2,
      colors, nullptr, 2, SkTileMode::kClamp));
  canvas->DrawRoundRect(gfx::ToEnclosingRect(local_bounds_f), 10, flags);
}

class LayerView : public View {
 public:
  LayerView() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetFillsBoundsCompletely(false);
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(10.f));
    layer()->set_delegate(&layer_delegate_);
  }

  ~LayerView() override = default;

 private:
  RoundedRectLayerPainter layer_delegate_ = {this};
};

class AnimationContainer : public View {
 public:
  AnimationContainer();
  ~AnimationContainer() override = default;

  // ButtonListener:
  void ButtonPressed();

 private:
  View* control_panel_;
  View* animation_panel_;
  LayerView* animation_view_ = nullptr;
};

AnimationContainer::AnimationContainer() {
  auto* layout = SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  animation_panel_ = AddChildView(std::make_unique<View>());
  control_panel_ = AddChildView(std::make_unique<View>());
  layout->SetFlexForView(animation_panel_, 3);
  layout->SetFlexForView(control_panel_, 1);
  auto* button =
      control_panel_->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&AnimationContainer::ButtonPressed,
                              base::Unretained(this)),
          u"Animate"));
  button->SetBoundsRect(
      gfx::Rect(gfx::Point(10, 10), button->GetPreferredSize()));
  animation_view_ =
      animation_panel_->AddChildView(std::make_unique<LayerView>());
  animation_view_->layer()->SetAnimator(
      new ui::LayerAnimator(base::TimeDelta::FromSeconds(2)));
  animation_view_->layer()->GetAnimator()->set_tween_type(
      gfx::Tween::EASE_IN_OUT);
  animation_view_->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation_view_->SetBoundsRect(gfx::Rect(100, 100, 400, 200));
  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();
  opacity_sequence->set_is_repeating(true);
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreateOpacityElement(
      0.4f, base::TimeDelta::FromSeconds(2)));
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreateOpacityElement(
      0.9f, base::TimeDelta::FromSeconds(2)));
  animation_view_->layer()->GetAnimator()->StartAnimation(
      opacity_sequence.release());
}

void AnimationContainer::ButtonPressed() {
  if (animation_view_->width() < 400) {
    animation_view_->SetBoundsRect(gfx::Rect(100, 100, 400, 200));
  } else {
    animation_view_->SetBoundsRect(gfx::Rect(10, 10, 40, 20));
  }
}

}  // namespace

LayerAnimationExample::LayerAnimationExample()
    : ExampleBase("Layer Animation") {}

LayerAnimationExample::~LayerAnimationExample() = default;

void LayerAnimationExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::make_unique<AnimationContainer>());
}

}  // namespace examples
}  // namespace views

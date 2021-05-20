// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/animation_example.h"

#include <algorithm>
#include <memory>

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

AnimationExample::AnimationExample() : ExampleBase("Animation") {}

AnimationExample::~AnimationExample() = default;

class AnimatingSquare : public View {
 public:
  explicit AnimatingSquare(size_t index);
  AnimatingSquare(const AnimatingSquare&) = delete;
  AnimatingSquare& operator=(const AnimatingSquare&) = delete;
  ~AnimatingSquare() override = default;

 protected:
  // views::View override
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  int index_;
  int paint_counter_ = 0;
  gfx::FontList font_list_ =
      LayoutProvider::Get()->GetTypographyProvider().GetFont(
          style::CONTEXT_DIALOG_TITLE,
          style::STYLE_PRIMARY);
};

AnimatingSquare::AnimatingSquare(size_t index) : index_(index) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
  layer()->SetAnimator(new ui::LayerAnimator(base::TimeDelta::FromSeconds(1)));
  layer()->GetAnimator()->set_tween_type(gfx::Tween::EASE_IN_OUT);
  layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();
  opacity_sequence->set_is_repeating(true);
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreateOpacityElement(
      0.4f, base::TimeDelta::FromSeconds(2)));
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreateOpacityElement(
      0.9f, base::TimeDelta::FromSeconds(2)));
  layer()->GetAnimator()->StartAnimation(opacity_sequence.release());
}

void AnimatingSquare::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  const SkColor color = SkColorSetRGB((5 - index_) * 51, 0, index_ * 51);
  const SkColor colors[2] = {color,
                             color_utils::HSLShift(color, {-1.0, -1.0, 0.75})};
  cc::PaintFlags flags;
  gfx::Rect local_bounds = gfx::Rect(layer()->size());
  const float dsf = canvas->UndoDeviceScaleFactor();
  gfx::RectF local_bounds_f = gfx::RectF(local_bounds);
  local_bounds_f.Scale(dsf);
  SkRect bounds = gfx::RectToSkRect(gfx::ToEnclosingRect(local_bounds_f));
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeRadialGradient(
      SkPoint::Make(bounds.centerX(), bounds.centerY()), bounds.width() / 2,
      colors, nullptr, 2, SkTileMode::kClamp));
  canvas->DrawRect(gfx::ToEnclosingRect(local_bounds_f), flags);
  int width = 0;
  int height = 0;
  std::u16string counter = base::NumberToString16(++paint_counter_);
  canvas->SizeStringInt(counter, font_list_, &width, &height, 0,
                        gfx::Canvas::TEXT_ALIGN_CENTER);
  local_bounds.ClampToCenteredSize(gfx::Size(width, height));
  canvas->DrawStringRectWithFlags(counter, font_list_, SK_ColorBLACK,
                                  local_bounds, gfx::Canvas::TEXT_ALIGN_CENTER);
}

class SquaresLayoutManager : public LayoutManagerBase {
 public:
  SquaresLayoutManager() = default;
  ~SquaresLayoutManager() override = default;

 protected:
  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

 private:
  static constexpr int kPadding = 25;
  static constexpr gfx::Size kSize = gfx::Size(100, 100);
};

// static
constexpr gfx::Size SquaresLayoutManager::kSize;

ProposedLayout SquaresLayoutManager::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;

  const auto& children = host_view()->children();
  const int item_width = kSize.width() + kPadding;
  const int item_height = kSize.height() + kPadding;
  const int max_width = kPadding + (children.size() * item_width);
  const int bounds_width =
      std::max(kPadding + item_width, size_bounds.width().min_of(max_width));
  const int views_per_row = (bounds_width - kPadding) / item_width;

  for (size_t i = 0; i < children.size(); ++i) {
    const size_t row = i / views_per_row;
    const size_t column = i % views_per_row;
    const gfx::Point origin(kPadding + column * item_width,
                            kPadding + row * item_height);
    layout.child_layouts.push_back(
        {children[i], true, gfx::Rect(origin, kSize), SizeBounds(kSize)});
  }

  const size_t num_rows = (children.size() + views_per_row - 1) / views_per_row;
  const int max_height = kPadding + (num_rows * item_height);
  const int bounds_height =
      std::max(kPadding + item_height, size_bounds.height().min_of(max_height));
  layout.host_size = {bounds_width, bounds_height};
  return layout;
}

void AnimationExample::CreateExampleView(View* container) {
  container->SetBackground(CreateSolidBackground(SK_ColorWHITE));
  container->SetPaintToLayer();
  container->layer()->SetMasksToBounds(true);
  container->layer()->SetFillsBoundsOpaquely(true);

  container->SetLayoutManager(std::make_unique<SquaresLayoutManager>());
  for (size_t i = 0; i < 5; ++i)
    container->AddChildView(std::make_unique<AnimatingSquare>(i));
}

}  // namespace examples
}  // namespace views

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/animation_example.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"

namespace views::examples {

AnimationExample::AnimationExample() : ExampleBase("Animation") {}

AnimationExample::~AnimationExample() = default;

class AnimatingSquare : public View {
  METADATA_HEADER(AnimatingSquare, View)

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
      TypographyProvider::Get().GetFont(style::CONTEXT_DIALOG_TITLE,
                                        style::STYLE_PRIMARY);
};

BEGIN_METADATA(AnimatingSquare)
END_METADATA

AnimatingSquare::AnimatingSquare(size_t index) : index_(index) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
}

void AnimatingSquare::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  const SkColor color = SkColorSetRGB((5 - index_) * 51, 0, index_ * 51);
  // TODO(crbug.com/40219248): Remove this FromColor and make all SkColor4f.
  const SkColor4f colors[2] = {
      SkColor4f::FromColor(color),
      SkColor4f::FromColor(color_utils::HSLShift(color, {-1.0, -1.0, 0.75}))};
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
  canvas->DrawStringRectWithFlags(
      counter, font_list_,
      GetColorProvider()->GetColor(
          ExamplesColorIds::kColorAnimationExampleForeground),
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
  void LayoutImpl() override;
  void OnInstalled(View* host) override;

 private:
  static constexpr int kPadding = 25;
  static constexpr gfx::Size kSize = gfx::Size(100, 100);

  std::unique_ptr<BoundsAnimator> bounds_animator_;
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
        {children[i].get(), true, gfx::Rect(origin, kSize), SizeBounds(kSize)});
  }

  const size_t num_rows = (children.size() + views_per_row - 1) / views_per_row;
  const int max_height = kPadding + (num_rows * item_height);
  const int bounds_height =
      std::max(kPadding + item_height, size_bounds.height().min_of(max_height));
  layout.host_size = {bounds_width, bounds_height};
  return layout;
}

void SquaresLayoutManager::LayoutImpl() {
  ProposedLayout proposed_layout = GetProposedLayout(host_view()->size());
  for (auto child_layout : proposed_layout.child_layouts) {
    bounds_animator_->AnimateViewTo(child_layout.child_view,
                                    child_layout.bounds);
  }
}

void SquaresLayoutManager::OnInstalled(View* host) {
  bounds_animator_ = std::make_unique<BoundsAnimator>(host, true);
  bounds_animator_->SetAnimationDuration(base::Seconds(1));
}

void AnimationExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(), 10));

  View* squares_container = container->AddChildView(std::make_unique<View>());
  squares_container->SetBackground(CreateThemedSolidBackground(
      ExamplesColorIds::kColorAnimationExampleBackground));
  squares_container->SetPaintToLayer();
  squares_container->layer()->SetMasksToBounds(true);
  squares_container->layer()->SetFillsBoundsOpaquely(true);

  squares_container->SetLayoutManager(std::make_unique<SquaresLayoutManager>());
  for (size_t i = 0; i < 5; ++i)
    squares_container->AddChildView(std::make_unique<AnimatingSquare>(i));

  {
    gfx::RoundedCornersF rounded_corners(12.0f, 12.0f, 12.0f, 12.0f);
    AnimationBuilder b;
    abort_handle_ = b.GetAbortHandle();
    for (views::View* view : squares_container->children()) {
      b.Once()
          .SetDuration(base::Seconds(10))
          .SetRoundedCorners(view, rounded_corners);
      b.Repeatedly()
          .SetDuration(base::Seconds(2))
          .SetOpacity(view, 0.4f, gfx::Tween::LINEAR_OUT_SLOW_IN)
          .Then()
          .SetDuration(base::Seconds(2))
          .SetOpacity(view, 0.9f, gfx::Tween::EASE_OUT_3);
    }
  }

  container->AddChildView(std::make_unique<MdTextButton>(
      base::BindRepeating(
          [](std::unique_ptr<AnimationAbortHandle>* abort_handle) {
            abort_handle->reset();
          },
          &abort_handle_),
      l10n_util::GetStringUTF16(IDS_ABORT_ANIMATION_BUTTON)));
}

}  // namespace views::examples

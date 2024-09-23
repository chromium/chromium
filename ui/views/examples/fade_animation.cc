// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/fade_animation.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/examples_themed_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace views::examples {

class CenteringLayoutManager : public LayoutManagerBase {
 public:
  CenteringLayoutManager() = default;
  ~CenteringLayoutManager() override = default;

 protected:
  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;
};

constexpr gfx::Size FadingView::kSize;

FadingView::FadingView() {
  Builder<FadingView>(this)
      .SetUseDefaultFillLayout(true)
      .SetPreferredSize(kSize)
      .AddChildren(
          Builder<BoxLayoutView>()
              .CopyAddressTo(&primary_view_)
              .SetBorder(CreateThemedRoundedRectBorder(
                  1, kCornerRadius,
                  ExamplesColorIds::kColorFadeAnimationExampleBorder))
              .SetBackground(CreateThemedRoundedRectBackground(
                  ExamplesColorIds::kColorFadeAnimationExampleBackground,
                  kCornerRadius, 1))
              .SetPaintToLayer()
              .SetOrientation(BoxLayout::Orientation::kVertical)
              .SetMainAxisAlignment(BoxLayout::MainAxisAlignment::kCenter)
              .SetBetweenChildSpacing(kSpacing)
              .AddChildren(Builder<Label>()
                               .SetText(u"Primary Title")
                               .SetTextContext(style::CONTEXT_DIALOG_TITLE)
                               .SetTextStyle(style::STYLE_PRIMARY)
                               .SetVerticalAlignment(gfx::ALIGN_MIDDLE),
                           Builder<Label>()
                               .SetText(u"Secondary Title")
                               .SetTextContext(style::CONTEXT_LABEL)
                               .SetTextStyle(style::STYLE_SECONDARY)
                               .SetVerticalAlignment(gfx::ALIGN_MIDDLE)),
          Builder<BoxLayoutView>()
              .CopyAddressTo(&secondary_view_)
              .SetBorder(CreateThemedRoundedRectBorder(
                  1, kCornerRadius,
                  ExamplesColorIds::kColorFadeAnimationExampleBorder))
              .SetBackground(CreateThemedRoundedRectBackground(
                  ExamplesColorIds::kColorFadeAnimationExampleBackground,
                  kCornerRadius, 1))
              .SetPaintToLayer()
              .SetOrientation(BoxLayout::Orientation::kVertical)
              .SetMainAxisAlignment(BoxLayout::MainAxisAlignment::kCenter)
              .SetBetweenChildSpacing(kSpacing)
              .AddChild(Builder<ThemedLabel>()
                            .SetText(u"Working...")
                            .SetTextContext(style::CONTEXT_DIALOG_TITLE)
                            .SetTextStyle(style::STYLE_PRIMARY)
                            .SetVerticalAlignment(gfx::ALIGN_MIDDLE)
                            .SetEnabledColorId(
                                ExamplesColorIds::
                                    kColorFadeAnimationExampleForeground)))
      .BuildChildren();
  primary_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadiusF));
  secondary_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadiusF));
  secondary_view_->layer()->SetOpacity(0.0f);

  AnimationBuilder()
      .Repeatedly()
      .Offset(base::Seconds(2))
      .SetDuration(base::Seconds(1))
      .SetOpacity(primary_view_, 0.0f)
      .SetOpacity(secondary_view_, 1.0f)
      .Offset(base::Seconds(2))
      .SetDuration(base::Seconds(1))
      .SetOpacity(primary_view_, 1.0f)
      .SetOpacity(secondary_view_, 0.0f);
}

FadingView::~FadingView() = default;

BEGIN_METADATA(FadingView)
END_METADATA

ProposedLayout CenteringLayoutManager::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;
  const auto& children = host_view()->children();

  gfx::Rect host_bounds(size_bounds.width().min_of(host_view()->width()),
                        size_bounds.height().min_of(host_view()->height()));
  for (views::View* child : children) {
    gfx::Size preferred_size = child->GetPreferredSize(size_bounds);
    gfx::Rect child_bounds = host_bounds;
    child_bounds.ClampToCenteredSize(preferred_size);

    layout.child_layouts.push_back(
        {child, true, child_bounds, SizeBounds(preferred_size)});
  }
  layout.host_size = host_bounds.size();
  return layout;
}

FadeAnimationExample::FadeAnimationExample() : ExampleBase("Fade Animation") {}

FadeAnimationExample::~FadeAnimationExample() = default;

void FadeAnimationExample::CreateExampleView(View* container) {
  container->SetBackground(CreateThemedSolidBackground(
      ExamplesColorIds::kColorFadeAnimationExampleBackground));
  container->SetLayoutManager(std::make_unique<CenteringLayoutManager>());
  container->AddChildView(std::make_unique<FadingView>());
}

}  // namespace views::examples

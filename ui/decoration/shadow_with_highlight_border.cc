// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/shadow_with_highlight_border.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "ui/decoration/decoration_util.h"
#include "ui/decoration/highlight_border_value.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_value.h"

namespace ui::decoration {

DEFINE_SAFE_CAST_TARGET(ShadowWithHighlightBorder)

ShadowWithHighlightBorder::ShadowWithHighlightBorder(
    std::unique_ptr<Shadow> shadow,
    std::unique_ptr<HighlightBorder> highlight_border) {
  SetShadow(std::move(shadow));
  SetHighlightBorder(std::move(highlight_border));
}

ShadowWithHighlightBorder::~ShadowWithHighlightBorder() = default;

void ShadowWithHighlightBorder::SetShadow(std::unique_ptr<Shadow> shadow) {
  if (shadow_ == shadow) {
    return;
  }

  if (shadow_) {
    shadow_->set_details_changed_callback(base::NullCallback());
  }
  shadow_ = std::move(shadow);
  if (shadow_) {
    shadow_->set_details_changed_callback(
        base::BindRepeating(&ShadowWithHighlightBorder::NotifyDecorationChanged,
                            base::Unretained(this)));
  }
  NotifyDecorationChanged();
}

void ShadowWithHighlightBorder::SetHighlightBorder(
    std::unique_ptr<HighlightBorder> highlight_border) {
  if (highlight_border_ == highlight_border) {
    return;
  }

  if (highlight_border_) {
    highlight_border_->set_details_changed_callback(base::NullCallback());
  }
  highlight_border_ = std::move(highlight_border);
  if (highlight_border_) {
    highlight_border_->set_details_changed_callback(
        base::BindRepeating(&ShadowWithHighlightBorder::NotifyDecorationChanged,
                            base::Unretained(this)));
  }
  NotifyDecorationChanged();
}

std::optional<ShadowWithHighlightBorderValue>
ShadowWithHighlightBorder::GetSpec(
    const gfx::Rect& content_bounds,
    const gfx::RoundedCornersF& rounded_corners) const {
  CHECK(!content_bounds.IsEmpty());

  ShadowWithHighlightBorderValue value(
      shadow_ ? shadow_->GetSpec(content_bounds, rounded_corners)
              : std::nullopt,
      highlight_border_
          ? highlight_border_->GetSpec(content_bounds, rounded_corners)
          : std::nullopt);
  if (value.IsEmpty()) {
    return std::nullopt;
  }

  return value;
}

std::optional<DecorationSource::Details> ShadowWithHighlightBorder::GetDetails(
    const gfx::Rect& content_bounds,
    const gfx::RoundedCornersF& rounded_corners) {
  const std::optional<ShadowWithHighlightBorderValue> value =
      GetSpec(content_bounds, rounded_corners);
  if (!value.has_value()) {
    return std::nullopt;
  }

  const ShadowWithHighlightBorderDetails& details =
      ShadowWithHighlightBorderDetails::Get(rounded_corners, *value);

  // The highlight border's two rings sit inside the content, so only the
  // content inside the border patches is guaranteed to be covered.
  const auto& highlight_border = value->Get<HighlightBorderValue>();
  gfx::Rect occlusion_rect(content_bounds.size());
  occlusion_rect.Inset(highlight_border.has_value()
                           ? HighlightBorderGenerator::GetNineboxApertureInsets(
                                 *highlight_border, rounded_corners)
                           : GetInsetsForRoundedCorners(rounded_corners));

  return Details{
      .appearance =
          {
              .nine_patch_image = details.nine_patch_image,
              .aperture_insets = details.aperture_insets,
              .margins = details.margins,
          },
      .occlusion_rect = occlusion_rect,
  };
}

}  // namespace ui::decoration

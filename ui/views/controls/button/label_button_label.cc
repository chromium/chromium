// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_label.h"

#include <optional>
#include <string>
#include <string_view>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_variant.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace views::internal {

LabelButtonLabel::LabelButtonLabel(std::u16string_view text, int text_context)
    : Label(text, text_context, style::STYLE_PRIMARY) {}

LabelButtonLabel::~LabelButtonLabel() = default;

void LabelButtonLabel::SetDisabledColor(SkColor color) {
  requested_disabled_color_ = color;
  if (!GetEnabled()) {
    Label::SetEnabledColor(color);
  }
}

void LabelButtonLabel::SetDisabledColorId(std::optional<ui::ColorId> color_id) {
  if (!color_id.has_value()) {
    return;
  }
  requested_disabled_color_ = color_id.value();
  if (!GetEnabled()) {
    Label::SetEnabledColorId(color_id.value());
  }
}

std::optional<ui::ColorId> LabelButtonLabel::GetDisabledColorId() const {
  return requested_disabled_color_ ? requested_disabled_color_->GetColorId()
                                   : std::nullopt;
}

void LabelButtonLabel::SetEnabledColor(SkColor color) {
  requested_enabled_color_ = color;
  if (GetEnabled()) {
    Label::SetEnabledColor(color);
  }
}

void LabelButtonLabel::SetEnabledColorId(std::optional<ui::ColorId> color_id) {
  if (!color_id.has_value()) {
    return;
  }
  requested_enabled_color_ = color_id.value();
  if (GetEnabled()) {
    Label::SetEnabledColorId(color_id.value());
  }
}

std::optional<ui::ColorId> LabelButtonLabel::GetEnabledColorId() const {
  return requested_enabled_color_ ? requested_enabled_color_->GetColorId()
                                  : std::nullopt;
}

void LabelButtonLabel::OnThemeChanged() {
  SetColorForEnableState();
  Label::OnThemeChanged();
}

void LabelButtonLabel::OnEnabledChanged() {
  SetColorForEnableState();
}

void LabelButtonLabel::SetColorForEnableState() {
  const auto& color_variant =
      GetEnabled() ? requested_enabled_color_ : requested_disabled_color_;

  if (color_variant) {
    if (auto color = color_variant->GetSkColor()) {
      Label::SetEnabledColor(*color);
    } else {
      Label::SetEnabledColorId(*color_variant->GetColorId());
    }
  } else {
    // Get default color Id.
    const ui::ColorId default_color_id = TypographyProvider::Get().GetColorId(
        GetTextContext(),
        GetEnabled() ? style::STYLE_PRIMARY : style::STYLE_DISABLED);
    // Set default color Id.
    Label::SetEnabledColorId(default_color_id);
  }
}

BEGIN_METADATA(LabelButtonLabel)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, EnabledColorId)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, DisabledColorId)
END_METADATA

}  // namespace views::internal

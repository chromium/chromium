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

void LabelButtonLabel::SetDisabledColor(ui::ColorVariant color) {
  requested_disabled_color_ = color;
  if (!GetEnabledInViewsSubtree()) {
    Label::SetEnabledColor(color);
  }
}

std::optional<ui::ColorVariant> LabelButtonLabel::GetDisabledColor() const {
  return requested_disabled_color_;
}

void LabelButtonLabel::SetEnabledColor(ui::ColorVariant color) {
  requested_enabled_color_ = color;
  if (GetEnabledInViewsSubtree()) {
    Label::SetEnabledColor(color);
  }
}

std::optional<ui::ColorVariant> LabelButtonLabel::GetEnabledColor() const {
  return requested_enabled_color_;
}

void LabelButtonLabel::OnThemeChanged() {
  SetColorForEnableState();
  Label::OnThemeChanged();
}

void LabelButtonLabel::OnEnabledChanged() {
  SetColorForEnableState();
}

void LabelButtonLabel::SetColorForEnableState() {
  const auto& color_variant = GetEnabledInViewsSubtree()
                                  ? requested_enabled_color_
                                  : requested_disabled_color_;

  if (color_variant) {
    Label::SetEnabledColor(*color_variant);
  } else {
    // Get default color Id.
    const ui::ColorId default_color_id = TypographyProvider::Get().GetColorId(
        GetTextContext(), GetEnabledInViewsSubtree() ? style::STYLE_PRIMARY
                                                     : style::STYLE_DISABLED);
    // Set default color Id.
    Label::SetEnabledColor(default_color_id);
  }
}

BEGIN_METADATA(LabelButtonLabel)
ADD_READONLY_PROPERTY_METADATA(std::optional<ui::ColorVariant>, EnabledColor)
ADD_READONLY_PROPERTY_METADATA(std::optional<ui::ColorVariant>, DisabledColor)
END_METADATA

}  // namespace views::internal

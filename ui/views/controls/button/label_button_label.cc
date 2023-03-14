// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_label.h"

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/layout_provider.h"

namespace views::internal {

LabelButtonLabel::LabelButtonLabel(const std::u16string& text, int text_context)
    : Label(text, text_context, style::STYLE_PRIMARY) {}

LabelButtonLabel::~LabelButtonLabel() = default;

void LabelButtonLabel::SetDisabledColor(SkColor color) {
  requested_disabled_color_ = color;
  if (!GetEnabled())
    Label::SetEnabledColor(color);
}

void LabelButtonLabel::SetDisabledColorId(
    absl::optional<ui::ColorId> color_id) {
  if (!color_id.has_value()) {
    return;
  }
  requested_disabled_color_ = color_id.value();
  if (!GetEnabled()) {
    Label::SetEnabledColorId(color_id.value());
  }
}

absl::optional<ui::ColorId> LabelButtonLabel::GetDisabledColorId() const {
  if (absl::holds_alternative<ui::ColorId>(requested_disabled_color_)) {
    return absl::get<ui::ColorId>(requested_disabled_color_);
  }
  return absl::nullopt;
}

void LabelButtonLabel::SetEnabledColor(SkColor color) {
  requested_enabled_color_ = color;
  if (GetEnabled())
    Label::SetEnabledColor(color);
}

void LabelButtonLabel::SetEnabledColorId(absl::optional<ui::ColorId> color_id) {
  if (!color_id.has_value()) {
    return;
  }
  requested_enabled_color_ = color_id.value();
  if (GetEnabled()) {
    Label::SetEnabledColorId(color_id.value());
  }
}

absl::optional<ui::ColorId> LabelButtonLabel::GetEnabledColorId() const {
  if (absl::holds_alternative<ui::ColorId>(requested_enabled_color_)) {
    return absl::get<ui::ColorId>(requested_enabled_color_);
  }
  return absl::nullopt;
}

void LabelButtonLabel::OnThemeChanged() {
  SetColorForEnableState();
  Label::OnThemeChanged();
}

void LabelButtonLabel::OnEnabledChanged() {
  SetColorForEnableState();
}

void LabelButtonLabel::SetColorForEnableState() {
  const absl::variant<absl::monostate, SkColor, ui::ColorId>& color =
      GetEnabled() ? requested_enabled_color_ : requested_disabled_color_;
  if (absl::holds_alternative<SkColor>(color)) {
    Label::SetEnabledColor(absl::get<SkColor>(color));
  } else if (absl::holds_alternative<ui::ColorId>(color)) {
    Label::SetEnabledColorId(absl::get<ui::ColorId>(color));
  } else {
    // Get default color Id.
    const ui::ColorId default_color_id =
        LayoutProvider::Get()->GetTypographyProvider().GetColorId(
            GetTextContext(),
            GetEnabled() ? style::STYLE_PRIMARY : style::STYLE_DISABLED);
    // Set default color Id.
    Label::SetEnabledColorId(default_color_id);
  }
}

BEGIN_METADATA(LabelButtonLabel, Label)
ADD_PROPERTY_METADATA(absl::optional<ui::ColorId>, EnabledColorId)
ADD_PROPERTY_METADATA(absl::optional<ui::ColorId>, DisabledColorId)
END_METADATA

}  // namespace views::internal

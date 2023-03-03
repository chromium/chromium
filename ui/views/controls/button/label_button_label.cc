// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_label.h"

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"

namespace views::internal {

LabelButtonLabel::LabelButtonLabel(const std::u16string& text, int text_context)
    : Label(text, text_context, style::STYLE_PRIMARY) {}

LabelButtonLabel::~LabelButtonLabel() = default;

void LabelButtonLabel::SetDisabledColor(SkColor color) {
  requested_disabled_color_ = color;
  if (!GetEnabled())
    Label::SetEnabledColor(color);
}

void LabelButtonLabel::SetEnabledColor(SkColor color) {
  requested_enabled_color_ = color;
  if (GetEnabled())
    Label::SetEnabledColor(color);
}

void LabelButtonLabel::OnThemeChanged() {
  SetColorForEnableState();
  Label::OnThemeChanged();
}

void LabelButtonLabel::OnEnabledChanged() {
  SetColorForEnableState();
}

void LabelButtonLabel::SetColorForEnableState() {
  const absl::optional<SkColor>& color =
      GetEnabled() ? requested_enabled_color_ : requested_disabled_color_;
  if (color) {
    Label::SetEnabledColor(*color);
  } else if (GetWidget()) {
    // If there is no widget, we can't actually get the colors here.
    // An OnThemeChanged() will fire once a widget is available.
    Label::SetEnabledColor(GetColorProvider()->GetColor(style::GetColorId(
        GetTextContext(),
        GetEnabled() ? style::STYLE_PRIMARY : style::STYLE_DISABLED)));
  }
}

BEGIN_METADATA(LabelButtonLabel, Label)
END_METADATA

}  // namespace views::internal

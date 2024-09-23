// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cascading_property.h"

#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                       views::CascadingProperty<SkColor>*)

namespace views {
namespace {

class CascadingColorProviderColor final : public CascadingProperty<SkColor> {
 public:
  explicit CascadingColorProviderColor(ui::ColorId color_id)
      : color_id_(color_id) {}

  // CascadingProperty<SkColor>:
  SkColor GetValue(const View* view) const override {
    return view->GetColorProvider()->GetColor(color_id_);
  }

 private:
  const ui::ColorId color_id_;
};

}  // namespace

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(CascadingProperty<SkColor>,
                                   kCascadingBackgroundColor,
                                   nullptr)

void SetCascadingColorProviderColor(
    views::View* view,
    const ui::ClassProperty<CascadingProperty<SkColor>*>* property_key,
    ui::ColorId color_id) {
  SetCascadingProperty(
      view, property_key,
      std::make_unique<views::CascadingColorProviderColor>(color_id));
}

SkColor GetCascadingBackgroundColor(View* view) {
  const std::optional<SkColor> color =
      GetCascadingProperty(view, kCascadingBackgroundColor);
  return color.value_or(
      view->GetColorProvider()->GetColor(ui::kColorWindowBackground));
}

SkColor GetCascadingAccentColor(View* view) {
  const SkColor default_color =
      view->GetColorProvider()->GetColor(ui::kColorFocusableBorderFocused);
  const SkColor background_color = GetCascadingBackgroundColor(view);
  return color_utils::BlendForMinContrast(
             default_color, background_color, std::nullopt,
             color_utils::kMinimumVisibleContrastRatio)
      .color;
}

}  // namespace views

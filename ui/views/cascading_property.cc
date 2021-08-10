// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cascading_property.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                       views::CascadingProperty<SkColor>*)

namespace views {

namespace {
class CascadingThemeProviderColor final : public CascadingProperty<SkColor> {
 public:
  explicit CascadingThemeProviderColor(int color_id) : color_id_(color_id) {}

  // CascadingProperty<SkColor>:
  SkColor GetValue(const View* view) const override {
    return view->GetThemeProvider()->GetColor(color_id_);
  }

 private:
  const int color_id_;
};

class CascadingNativeThemeColor final : public CascadingProperty<SkColor> {
 public:
  explicit CascadingNativeThemeColor(ui::NativeTheme::ColorId color_id)
      : color_id_(color_id) {}

  // CascadingProperty<SkColor>:
  SkColor GetValue(const View* view) const override {
    return view->GetNativeTheme()->GetSystemColor(color_id_);
  }

 private:
  const ui::NativeTheme::ColorId color_id_;
};
}  // namespace

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(CascadingProperty<SkColor>,
                                   kCascadingBackgroundColor,
                                   nullptr)

void SetCascadingThemeProviderColor(
    views::View* view,
    const ui::ClassProperty<CascadingProperty<SkColor>*>* property_key,
    int color_id) {
  SetCascadingProperty(
      view, property_key,
      std::make_unique<views::CascadingThemeProviderColor>(color_id));
}

void SetCascadingNativeThemeColor(
    views::View* view,
    const ui::ClassProperty<CascadingProperty<SkColor>*>* property_key,
    ui::NativeTheme::ColorId color_id) {
  SetCascadingProperty(
      view, property_key,
      std::make_unique<views::CascadingNativeThemeColor>(color_id));
}

SkColor GetCascadingBackgroundColor(View* view) {
  const absl::optional<SkColor> color =
      GetCascadingProperty(view, kCascadingBackgroundColor);
  return color.value_or(view->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_WindowBackground));
}

SkColor GetCascadingAccentColor(View* view) {
  const SkColor default_color = view->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_FocusedBorderColor);

  return color_utils::PickGoogleColor(
      default_color, GetCascadingBackgroundColor(view),
      color_utils::kMinimumVisibleContrastRatio);
}

}  // namespace views

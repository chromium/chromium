// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"

namespace views {

MdTextButton::MdTextButton(PressedCallback callback,
                           const base::string16& text,
                           int button_context)
    : LabelButton(std::move(callback), text, button_context) {
  SetInkDropMode(InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetShowInkDropWhenHotTracked(true);
  SetCornerRadius(LayoutProvider::Get()->GetCornerRadiusMetric(EMPHASIS_LOW));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetFocusForPlatform();

  const int minimum_width = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH);
  SetMinSize(gfx::Size(minimum_width, 0));
  SetInstallFocusRingOnFocus(true);
  label()->SetAutoColorReadabilityEnabled(false);
  SetRequestFocusOnPress(false);
  SetAnimateOnStateChange(true);

  // Paint to a layer so that the canvas is snapped to pixel boundaries (useful
  // for fractional DSF).
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Call this to calculate the border given text.
  UpdatePadding();
}

MdTextButton::MdTextButton(ButtonListener* listener,
                           const base::string16& text,
                           int button_context)
    : MdTextButton(PressedCallback(listener, this), text, button_context) {}

MdTextButton::~MdTextButton() = default;

void MdTextButton::SetProminent(bool is_prominent) {
  if (is_prominent_ == is_prominent)
    return;

  is_prominent_ = is_prominent;
  UpdateColors();
  OnPropertyChanged(&is_prominent_, kPropertyEffectsNone);
}

bool MdTextButton::GetProminent() const {
  return is_prominent_;
}

void MdTextButton::SetBgColorOverride(const base::Optional<SkColor>& color) {
  if (color == bg_color_override_)
    return;
  bg_color_override_ = color;
  UpdateColors();
  OnPropertyChanged(&bg_color_override_, kPropertyEffectsNone);
}

base::Optional<SkColor> MdTextButton::GetBgColorOverride() const {
  return bg_color_override_;
}

void MdTextButton::SetCornerRadius(float radius) {
  if (corner_radius_ == radius)
    return;
  corner_radius_ = radius;
  SetInkDropSmallCornerRadius(corner_radius_);
  SetInkDropLargeCornerRadius(corner_radius_);
  OnPropertyChanged(&corner_radius_, kPropertyEffectsPaint);
}

float MdTextButton::GetCornerRadius() const {
  return corner_radius_;
}

void MdTextButton::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  UpdateColors();
}

SkColor MdTextButton::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(label()->GetEnabledColor());
}

void MdTextButton::StateChanged(ButtonState old_state) {
  LabelButton::StateChanged(old_state);
  UpdateColors();
}

void MdTextButton::OnFocus() {
  LabelButton::OnFocus();
  UpdateColors();
}

void MdTextButton::OnBlur() {
  LabelButton::OnBlur();
  UpdateColors();
}

std::unique_ptr<views::InkDropHighlight> MdTextButton::CreateInkDropHighlight()
    const {
  const ui::NativeTheme* theme = GetNativeTheme();
  // The prominent button hover effect is a shadow.
  constexpr int kYOffset = 1;
  constexpr int kSkiaBlurRadius = 2;
  ui::NativeTheme::ColorId fill_color_id;
  ui::NativeTheme::ColorId shadow_color_id =
      is_prominent_
          ? ui::NativeTheme::kColorId_ProminentButtonInkDropShadowColor
          : ui::NativeTheme::kColorId_ButtonInkDropShadowColor;
  if (GetState() == STATE_HOVERED) {
    fill_color_id = is_prominent_
                        ? ui::NativeTheme::kColorId_ProminentButtonHoverColor
                        : ui::NativeTheme::kColorId_ButtonHoverColor;
  } else {
    fill_color_id =
        is_prominent_
            ? ui::NativeTheme::kColorId_ProminentButtonInkDropFillColor
            : ui::NativeTheme::kColorId_ButtonInkDropFillColor;
  }
  std::vector<gfx::ShadowValue> shadows;
  // The notion of blur that gfx::ShadowValue uses is twice the Skia/CSS value.
  // Skia counts the number of pixels outside the mask area whereas
  // gfx::ShadowValue counts together the number of pixels inside and outside
  // the mask bounds.
  shadows.emplace_back(gfx::Vector2d(0, kYOffset), 2 * kSkiaBlurRadius,
                       theme->GetSystemColor(shadow_color_id));
  auto highlight = std::make_unique<InkDropHighlight>(
      gfx::RectF(GetLocalBounds()).CenterPoint(),
      std::make_unique<BorderShadowLayerDelegate>(
          shadows, GetLocalBounds(), theme->GetSystemColor(fill_color_id),
          corner_radius_));
  highlight->set_visible_opacity(1.0f);
  return highlight;
}

void MdTextButton::SetEnabledTextColors(base::Optional<SkColor> color) {
  LabelButton::SetEnabledTextColors(std::move(color));
  UpdateColors();
}

void MdTextButton::SetCustomPadding(const gfx::Insets& padding) {
  custom_padding_ = padding;
  UpdatePadding();
}

void MdTextButton::SetText(const base::string16& text) {
  LabelButton::SetText(text);
  UpdatePadding();
}

PropertyEffects MdTextButton::UpdateStyleToIndicateDefaultStatus() {
  is_prominent_ = is_prominent_ || GetIsDefault();
  UpdateColors();
  return kPropertyEffectsNone;
}

void MdTextButton::UpdatePadding() {
  // Don't use font-based padding when there's no text visible.
  if (GetText().empty()) {
    SetBorder(NullBorder());
    return;
  }

  SetBorder(
      CreateEmptyBorder(custom_padding_.value_or(CalculateDefaultPadding())));
}

gfx::Insets MdTextButton::CalculateDefaultPadding() const {
  // Text buttons default to 28dp in height on all platforms when the base font
  // is in use, but should grow or shrink if the font size is adjusted up or
  // down. When the system font size has been adjusted, the base font will be
  // larger than normal such that 28dp might not be enough, so also enforce a
  // minimum height of twice the font size.
  // Example 1:
  // * Normal button on ChromeOS, 12pt Roboto. Button height of 28dp.
  // * Button on ChromeOS that has been adjusted to 14pt Roboto. Button height
  // of 28 + 2 * 2 = 32dp.
  // * Linux user sets base system font size to 17dp. For a normal button, the
  // |size_delta| will be zero, so to adjust upwards we double 17 to get 34.
  int size_delta =
      label()->font_list().GetFontSize() -
      style::GetFont(style::CONTEXT_BUTTON_MD, style::STYLE_PRIMARY)
          .GetFontSize();
  // TODO(tapted): This should get |target_height| using LayoutProvider::
  // GetControlHeightForFont().
  constexpr int kBaseHeight = 32;
  int target_height = std::max(kBaseHeight + size_delta * 2,
                               label()->font_list().GetFontSize() * 2);

  int label_height = label()->GetPreferredSize().height();
  int top_padding = (target_height - label_height) / 2;
  int bottom_padding = (target_height - label_height + 1) / 2;
  DCHECK_EQ(target_height, label_height + top_padding + bottom_padding);

  // TODO(estade): can we get rid of the platform style border hoopla if
  // we apply the MD treatment to all buttons, even GTK buttons?
  const int horizontal_padding = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUTTON_HORIZONTAL_PADDING);
  return gfx::Insets(top_padding, horizontal_padding, bottom_padding,
                     horizontal_padding);
}

void MdTextButton::UpdateTextColor() {
  if (explicitly_set_normal_color())
    return;

  SkColor enabled_text_color =
      style::GetColor(*this, label()->GetTextContext(),
                      is_prominent_ ? style::STYLE_DIALOG_BUTTON_DEFAULT
                                    : style::STYLE_PRIMARY);

  const auto colors = explicitly_set_colors();
  LabelButton::SetEnabledTextColors(enabled_text_color);
  // Disabled buttons need the disabled color explicitly set.
  // This ensures that label()->GetEnabledColor() returns the correct color as
  // the basis for calculating the stroke color. enabled_text_color isn't used
  // since a descendant could have overridden the label enabled color.
  if (GetState() == STATE_DISABLED) {
    LabelButton::SetTextColor(STATE_DISABLED,
                              style::GetColor(*this, label()->GetTextContext(),
                                              style::STYLE_DISABLED));
  }
  set_explicitly_set_colors(colors);
}

void MdTextButton::UpdateBackgroundColor() {
  bool is_disabled = GetVisualState() == STATE_DISABLED;
  ui::NativeTheme* theme = GetNativeTheme();
  SkColor bg_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonColor);

  if (bg_color_override_) {
    bg_color = *bg_color_override_;
  } else if (is_prominent_) {
    bg_color = theme->GetSystemColor(
        HasFocus() ? ui::NativeTheme::kColorId_ProminentButtonFocusedColor
                   : ui::NativeTheme::kColorId_ProminentButtonColor);
    if (is_disabled) {
      bg_color = theme->GetSystemColor(
          ui::NativeTheme::kColorId_ProminentButtonDisabledColor);
    }
  }

  if (GetState() == STATE_PRESSED) {
    bg_color = theme->GetSystemButtonPressedColor(bg_color);
  }

  SkColor stroke_color;
  if (is_prominent_) {
    stroke_color = SK_ColorTRANSPARENT;
  } else {
    stroke_color = theme->GetSystemColor(
        is_disabled ? ui::NativeTheme::kColorId_DisabledButtonBorderColor
                    : ui::NativeTheme::kColorId_ButtonBorderColor);
  }

  SetBackground(
      CreateBackgroundFromPainter(Painter::CreateRoundRectWith1PxBorderPainter(
          bg_color, stroke_color, corner_radius_)));
}

void MdTextButton::UpdateColors() {
  UpdateTextColor();
  UpdateBackgroundColor();
  SchedulePaint();
}

BEGIN_METADATA(MdTextButton, LabelButton)
ADD_PROPERTY_METADATA(bool, Prominent)
ADD_PROPERTY_METADATA(float, CornerRadius)
ADD_PROPERTY_METADATA(base::Optional<SkColor>, BgColorOverride)
END_METADATA

}  // namespace views

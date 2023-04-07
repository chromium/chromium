// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"

namespace views {

MdTextButton::MdTextButton(PressedCallback callback,
                           const std::u16string& text,
                           int button_context)
    : LabelButton(std::move(callback), text, button_context) {
  InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetShowInkDropWhenHotTracked(true);
  InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      [](MdTextButton* host) { return host->GetHoverColor(host->GetStyle()); },
      this));

  if (features::IsChromeRefresh2023()) {
    constexpr int kImageSpacing = 8;
    SetImageLabelSpacing(kImageSpacing);
    // Highlight button colors already have opacity applied.
    // Set the opacity to 1 so the two values do not compound.
    InkDrop::Get(this)->SetHighlightOpacity(1);
  } else {
    SetCornerRadius(LayoutProvider::Get()->GetCornerRadiusMetric(
        ShapeContextTokens::kButtonRadius));
  }

  SetHorizontalAlignment(gfx::ALIGN_CENTER);

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

MdTextButton::~MdTextButton() = default;

void MdTextButton::SetProminent(bool is_prominent) {
  SetStyle(is_prominent ? Style::kProminent : Style::kDefault);
  UpdateColors();
}

bool MdTextButton::GetProminent() const {
  return style_ == Style::kProminent;
}

void MdTextButton::SetStyle(views::MdTextButton::Style button_style) {
  if (style_ == button_style) {
    return;
  }

  style_ = button_style;
  SetProperty(kDrawFocusRingBackgroundOutline,
              button_style == Style::kProminent);
  UpdateColors();
}

views::MdTextButton::Style MdTextButton::GetStyle() const {
  return style_;
}

SkColor MdTextButton::GetHoverColor(Style button_style) {
  if (!features::IsChromeRefresh2023()) {
    return color_utils::DeriveDefaultIconColor(label()->GetEnabledColor());
  }

  switch (button_style) {
    case Style::kProminent:
      return GetColorProvider()->GetColor(ui::kColorSysStateHoverOnProminent);
    case Style::kDefault:
    case Style::kText:
    case Style::kTonal:
    default:
      return GetColorProvider()->GetColor(ui::kColorSysStateHoverOnSubtle);
  }
}

void MdTextButton::SetBgColorOverride(const absl::optional<SkColor>& color) {
  if (color == bg_color_override_)
    return;
  bg_color_override_ = color;
  UpdateColors();
  OnPropertyChanged(&bg_color_override_, kPropertyEffectsNone);
}

absl::optional<SkColor> MdTextButton::GetBgColorOverride() const {
  return bg_color_override_;
}

void MdTextButton::SetCornerRadius(absl::optional<float> radius) {
  if (corner_radius_ == radius)
    return;
  corner_radius_ = radius;
  LabelButton::SetFocusRingCornerRadius(GetCornerRadiusValue());
  // UpdateColors also updates the background border radius.
  UpdateColors();
  OnPropertyChanged(&corner_radius_, kPropertyEffectsNone);
}

absl::optional<float> MdTextButton::GetCornerRadius() const {
  return corner_radius_;
}

float MdTextButton::GetCornerRadiusValue() const {
  return corner_radius_.value_or(0);
}

void MdTextButton::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  UpdateColors();
}

void MdTextButton::StateChanged(ButtonState old_state) {
  LabelButton::StateChanged(old_state);
  UpdateColors();
}

void MdTextButton::SetImageModel(ButtonState for_state,
                                 const ui::ImageModel& image_model) {
  LabelButton::SetImageModel(for_state, image_model);
  UpdatePadding();
}

void MdTextButton::OnFocus() {
  LabelButton::OnFocus();
  UpdateColors();
}

void MdTextButton::OnBlur() {
  LabelButton::OnBlur();
  UpdateColors();
}

void MdTextButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  LabelButton::OnBoundsChanged(previous_bounds);

  // A fully rounded corner radius is calculated based on the size of the
  // button. To avoid overriding a custom corner radius, make sure the default
  // radius is only called once by checking if the value already exists.
  if (!corner_radius_) {
    SetCornerRadius(LayoutProvider::Get()->GetCornerRadiusMetric(
        ShapeContextTokens::kButtonRadius, size()));
  }
}

void MdTextButton::SetEnabledTextColors(absl::optional<SkColor> color) {
  LabelButton::SetEnabledTextColors(std::move(color));
  UpdateColors();
}

void MdTextButton::SetCustomPadding(
    const absl::optional<gfx::Insets>& padding) {
  custom_padding_ = padding;
  UpdatePadding();
}

absl::optional<gfx::Insets> MdTextButton::GetCustomPadding() const {
  return custom_padding_.value_or(CalculateDefaultPadding());
}

void MdTextButton::SetText(const std::u16string& text) {
  LabelButton::SetText(text);
  UpdatePadding();
}

PropertyEffects MdTextButton::UpdateStyleToIndicateDefaultStatus() {
  SetProminent(style_ == Style::kProminent || GetIsDefault());
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
  int target_height = LayoutProvider::GetControlHeightForFont(
      label()->GetTextContext(), style::STYLE_PRIMARY, label()->font_list());

  int label_height = label()->GetPreferredSize().height();
  DCHECK_GE(target_height, label_height);
  int top_padding = (target_height - label_height) / 2;
  int bottom_padding = (target_height - label_height + 1) / 2;
  DCHECK_EQ(target_height, label_height + top_padding + bottom_padding);

  // TODO(estade): can we get rid of the platform style border hoopla if
  // we apply the MD treatment to all buttons, even GTK buttons?
  int right_padding = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUTTON_HORIZONTAL_PADDING);
  int left_padding = right_padding;
  if (HasImage(GetVisualState()) && features::IsChromeRefresh2023()) {
    constexpr int kLeftPadding = 12;
    left_padding = kLeftPadding;
  }
  return gfx::Insets::TLBR(top_padding, left_padding, bottom_padding,
                           right_padding);
}

void MdTextButton::UpdateTextColor() {
  if (explicitly_set_normal_color())
    return;

  style::TextStyle text_style = style::STYLE_PRIMARY;
  if (style_ == Style::kProminent) {
    text_style = style::STYLE_DIALOG_BUTTON_DEFAULT;
  } else if (style_ == Style::kTonal) {
    text_style = style::STYLE_DIALOG_BUTTON_TONAL;
  }

  const ui::ColorProvider* color_provider = GetColorProvider();
  SkColor enabled_text_color = color_provider->GetColor(
      style::GetColorId(label()->GetTextContext(), text_style));
  const auto colors = explicitly_set_colors();
  LabelButton::SetEnabledTextColors(enabled_text_color);
  // Disabled buttons need the disabled color explicitly set.
  // This ensures that label()->GetEnabledColor() returns the correct color as
  // the basis for calculating the stroke color. enabled_text_color isn't used
  // since a descendant could have overridden the label enabled color.
  if (GetState() == STATE_DISABLED) {
    LabelButton::SetTextColor(
        STATE_DISABLED, color_provider->GetColor(style::GetColorId(
                            label()->GetTextContext(), style::STYLE_DISABLED)));
  }
  set_explicitly_set_colors(colors);
}

void MdTextButton::UpdateBackgroundColor() {
  bool is_disabled = GetVisualState() == STATE_DISABLED;
  const ui::ColorProvider* color_provider = GetColorProvider();
  SkColor bg_color = color_provider->GetColor(ui::kColorButtonBackground);

  if (bg_color_override_) {
    bg_color = *bg_color_override_;
  } else if (style_ == Style::kProminent) {
    bg_color = color_provider->GetColor(
        HasFocus() ? ui::kColorButtonBackgroundProminentFocused
                   : ui::kColorButtonBackgroundProminent);
    if (is_disabled) {
      bg_color =
          color_provider->GetColor(ui::kColorButtonBackgroundProminentDisabled);
    }
  } else if (style_ == Style::kTonal) {
    bg_color = color_provider->GetColor(
        HasFocus() ? ui::kColorButtonBackgroundTonalFocused
                   : ui::kColorButtonBackgroundTonal);
    if (is_disabled) {
      bg_color =
          color_provider->GetColor(ui::kColorButtonBackgroundTonalDisabled);
    }
  }

  if (GetState() == STATE_PRESSED) {
    bg_color = GetNativeTheme()->GetSystemButtonPressedColor(bg_color);
  }

  SkColor stroke_color = color_provider->GetColor(
      is_disabled ? ui::kColorButtonBorderDisabled : ui::kColorButtonBorder);
  if (style_ == Style::kProminent || style_ == Style::kText ||
      style_ == Style::kTonal) {
    stroke_color = SK_ColorTRANSPARENT;
  }

  SetBackground(
      CreateBackgroundFromPainter(Painter::CreateRoundRectWith1PxBorderPainter(
          bg_color, stroke_color, GetCornerRadiusValue())));
}

void MdTextButton::UpdateColors() {
  if (GetWidget()) {
    UpdateTextColor();
    UpdateBackgroundColor();
    SchedulePaint();
  }
}

BEGIN_METADATA(MdTextButton, LabelButton)
ADD_PROPERTY_METADATA(bool, Prominent)
ADD_PROPERTY_METADATA(absl::optional<float>, CornerRadius)
ADD_PROPERTY_METADATA(absl::optional<SkColor>, BgColorOverride)
ADD_PROPERTY_METADATA(absl::optional<gfx::Insets>, CustomPadding)
END_METADATA

}  // namespace views

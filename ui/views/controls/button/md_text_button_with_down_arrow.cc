// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button_with_down_arrow.h"

#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/controls/button/md_text_button.h"

namespace views {
constexpr int kDropdownArrowSize = 20;

MdTextButtonWithDownArrow::MdTextButtonWithDownArrow(PressedCallback callback,
                                                     const std::u16string& text)
    : MdTextButton(std::move(callback), text) {
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  SetImageLabelSpacing(LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING));
  SetDropArrowImage();

  // Reduce padding between the drop arrow and the right border.
  const gfx::Insets original_padding = GetInsets();
  SetCustomPadding(
      gfx::Insets::TLBR(original_padding.top(),
                        LayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_DROPDOWN_BUTTON_LEFT_MARGIN),
                        original_padding.bottom(),
                        LayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN)));
}

MdTextButtonWithDownArrow::~MdTextButtonWithDownArrow() = default;

void MdTextButtonWithDownArrow::OnThemeChanged() {
  MdTextButton::OnThemeChanged();

  // The icon's color is derived from the label's |enabled_color|, which might
  // have changed as the result of the theme change.
  SetDropArrowImage();
}

void MdTextButtonWithDownArrow::StateChanged(ButtonState old_state) {
  MdTextButton::StateChanged(old_state);

  // A state change may require the arrow's color to be updated.
  SetDropArrowImage();
}

void MdTextButtonWithDownArrow::SetDropArrowImage() {
  SkColor drop_arrow_color = label()->GetEnabledColor();
  auto drop_arrow_image = ui::ImageModel::FromVectorIcon(
      kArrowDropDownIcon, drop_arrow_color, kDropdownArrowSize);
  SetImageModel(Button::STATE_NORMAL, drop_arrow_image);
}

BEGIN_METADATA(MdTextButtonWithDownArrow)
END_METADATA

}  // namespace views

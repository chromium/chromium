// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button_with_spinner.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_variant.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_label.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/style/typography.h"

namespace views {
constexpr int kSpinnerDiameter = 20;
constexpr int kSpinnerLabelSpacing = 8;

MdTextButtonWithSpinner::MdTextButtonWithSpinner(
    Button::PressedCallback callback,
    std::u16string_view text)
    : MdTextButton(std::move(callback), text) {
  spinner_ = AddChildView(std::make_unique<Throbber>(kSpinnerDiameter));
  SetText(text);
}

MdTextButtonWithSpinner::~MdTextButtonWithSpinner() {
  SetSpinnerVisible(false);
}

void MdTextButtonWithSpinner::SetSpinnerVisible(bool visible) {
  spinner_visible_ = visible;
  if (spinner_visible_) {
    spinner_->Start();
  } else {
    spinner_->Stop();
  }
  PreferredSizeChanged();
}

bool MdTextButtonWithSpinner::GetSpinnerVisible() const {
  return spinner_visible_;
}

gfx::Size MdTextButtonWithSpinner::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  auto size = views::MdTextButton::CalculatePreferredSize(available_size);
  if (GetSpinnerVisible()) {
    size.set_width(size.width() + kSpinnerDiameter + kSpinnerLabelSpacing);
  }
  return size;
}

views::ProposedLayout MdTextButtonWithSpinner::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts =
      views::MdTextButton::CalculateProposedLayout(size_bounds);
  if (!GetSpinnerVisible()) {
    return layouts;
  }

  views::ChildLayout* label_layout = nullptr;
  for (auto& child_layout : layouts.child_layouts) {
    if (child_layout.child_view == label()) {
      label_layout = &child_layout;
      break;
    }
  }
  DCHECK(label_layout);

  const int button_content_width =
      layouts.host_size.width() - GetInsets().left() - GetInsets().right();

  const int label_preferred_width = label_layout->bounds.width();

  // Calculate the total width of the spinner and label.
  const int preferred_spinner_label_width =
      kSpinnerDiameter + kSpinnerLabelSpacing + label_layout->bounds.width();

  int label_width;
  int spinner_label_width;

  // Adjust width if the available space is smaller than the preferred space.
  if (preferred_spinner_label_width > button_content_width) {
    // Calculate the maximum width available for the label after accounting
    // for the fixed spinner and spacing.
    int max_available_label_width =
        button_content_width - kSpinnerDiameter - kSpinnerLabelSpacing;

    // Ensure the available width for the label is not negative.
    label_width = std::max(0, max_available_label_width);

    // Update combined width to reflect the shrunk label width.
    spinner_label_width = kSpinnerDiameter + kSpinnerLabelSpacing + label_width;
  } else {
    label_width = label_preferred_width;
    spinner_label_width = preferred_spinner_label_width;
  }

  // Account for different horizontal alignments.
  int combo_start_x;
  auto horizontal_alignment = GetHorizontalAlignment();
  DCHECK_NE(gfx::ALIGN_TO_HEAD, horizontal_alignment);
  switch (horizontal_alignment) {
    case gfx::HorizontalAlignment::ALIGN_LEFT:
      combo_start_x = GetInsets().left();
      break;
    case gfx::HorizontalAlignment::ALIGN_CENTER:
      combo_start_x =
          GetInsets().left() + (button_content_width - spinner_label_width) / 2;
      break;
    case gfx::HorizontalAlignment::ALIGN_RIGHT:
      combo_start_x =
          GetInsets().left() + button_content_width - spinner_label_width;
      break;
    case gfx::HorizontalAlignment::ALIGN_TO_HEAD:
      NOTREACHED();
  }

  const int spinner_x = combo_start_x;
  const int spinner_y =
      label_layout->bounds.CenterPoint().y() - kSpinnerDiameter / 2;
  const gfx::Rect spinner_bounds(spinner_x, spinner_y, kSpinnerDiameter,
                                 kSpinnerDiameter);

  // Adjust label position relative to spinner.
  const int label_x = spinner_x + kSpinnerDiameter + kSpinnerLabelSpacing;
  label_layout->bounds.set_x(label_x);
  label_layout->bounds.set_width(label_width);

  layouts.child_layouts.emplace_back(
      const_cast<views::Throbber*>(spinner_.get()), GetSpinnerVisible(),
      spinner_bounds, views::SizeBounds());

  return layouts;
}

void MdTextButtonWithSpinner::UpdateSpinnerColor() {
  style::TextStyle text_style = style::STYLE_PRIMARY;
  if (GetStyle() == ui::ButtonStyle::kProminent) {
    text_style = style::STYLE_DIALOG_BUTTON_DEFAULT;
  } else if (GetStyle() == ui::ButtonStyle::kTonal) {
    text_style = style::STYLE_DIALOG_BUTTON_TONAL;
  }
  if (GetState() == STATE_DISABLED) {
    text_style = style::STYLE_DISABLED;
  }
  const auto& typography_provider = TypographyProvider::Get();
  spinner_->SetColorId(
      typography_provider.GetColorId(label()->GetTextContext(), text_style));
}

void MdTextButtonWithSpinner::UpdateColors() {
  MdTextButton::UpdateColors();
  UpdateSpinnerColor();
}

BEGIN_METADATA(MdTextButtonWithSpinner)
ADD_PROPERTY_METADATA(bool, SpinnerVisible);
END_METADATA

}  // namespace views

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_password_combobox.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/render_text.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"

namespace views {

namespace {

constexpr int kEyePaddingWidth = 4;

// Creates the eye-styled icon that serves as a button to toggle the password
// visibility.
std::unique_ptr<ToggleImageButton> CreateEye(
    ImageButton::PressedCallback callback) {
  auto button = Builder<ToggleImageButton>()
                    .SetInstallFocusRingOnFocus(true)
                    .SetRequestFocusOnPress(true)
                    .SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE)
                    .SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER)
                    .SetCallback(std::move(callback))
                    .SetBorder(CreateEmptyBorder(kEyePaddingWidth))
                    .Build();
  SetImageFromVectorIconWithColorId(button.get(), kEyeIcon, ui::kColorIcon,
                                    ui::kColorIconDisabled);
  SetToggledImageFromVectorIconWithColorId(
      button.get(), kEyeCrossedIcon, ui::kColorIcon, ui::kColorIconDisabled);

  ConfigureComboboxButtonInkDrop(button.get());
  // We need this so the eye icon is not covered when the combo box view is
  // hovered
  button->SetPaintToLayer();
  button->layer()->SetFillsBoundsOpaquely(false);

  return button;
}

class PasswordMenuDecorationStrategy
    : public EditableCombobox::MenuDecorationStrategy {
 public:
  explicit PasswordMenuDecorationStrategy(
      const EditablePasswordCombobox* parent)
      : parent_(parent) {
    DCHECK(parent);
  }

  std::u16string DecorateItemText(std::u16string text) const override {
    return parent_->ArePasswordsRevealed()
               ? text
               : std::u16string(text.length(),
                                gfx::RenderText::kPasswordReplacementChar);
  }

 private:
  const raw_ptr<const EditablePasswordCombobox> parent_;
};

}  // namespace

EditablePasswordCombobox::EditablePasswordCombobox() = default;

EditablePasswordCombobox::EditablePasswordCombobox(
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    int text_context,
    int text_style,
    bool display_arrow,
    Button::PressedCallback eye_callback)
    : EditableCombobox(std::move(combobox_model),
                       /*filter_on_edit=*/false,
                       /*show_on_empty=*/true,
                       text_context,
                       text_style,
                       display_arrow) {
  // By default, clicking on the eye reveals/hides passwords.
  if (!eye_callback) {
    eye_callback = base::BindRepeating(
        [](views::EditablePasswordCombobox* combobox_ptr) {
          combobox_ptr->RevealPasswords(!combobox_ptr->ArePasswordsRevealed());
        },
        base::Unretained(this));
  }

  // If there is no arrow for a dropdown element, then the eye is too close to
  // the border of the textarea - therefore add additional padding.
  std::unique_ptr<ToggleImageButton> eye = CreateEye(std::move(eye_callback));
  eye_ = eye.get();
  if (!display_arrow) {
    // Add the insets to an additional container instead of directly to the
    // button's border so that the focus ring around the pressed button is not
    // affected by this additional padding.
    auto container =
        Builder<BoxLayoutView>()
            .SetInsideBorderInsets(gfx::Insets().set_right(
                std::max(0, LayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING) -
                                kEyePaddingWidth)))
            .Build();
    container->AddChildView(std::move(eye));
    AddControlElement(std::move(container));
  } else {
    AddControlElement(std::move(eye));
  }

  GetTextfield().SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  SetMenuDecorationStrategy(
      std::make_unique<PasswordMenuDecorationStrategy>(this));
}

EditablePasswordCombobox::~EditablePasswordCombobox() = default;

void EditablePasswordCombobox::SetPasswordIconTooltips(
    const std::u16string& tooltip_text,
    const std::u16string& toggled_tooltip_text) {
  eye_->SetTooltipText(tooltip_text);
  eye_->SetToggledTooltipText(toggled_tooltip_text);
}

void EditablePasswordCombobox::RevealPasswords(bool revealed) {
  if (revealed == are_passwords_revealed_) {
    return;
  }
  are_passwords_revealed_ = revealed;
  GetTextfield().SetTextInputType(revealed ? ui::TEXT_INPUT_TYPE_TEXT
                                           : ui::TEXT_INPUT_TYPE_PASSWORD);
  eye_->SetToggled(revealed);
  UpdateMenu();
}

bool EditablePasswordCombobox::ArePasswordsRevealed() const {
  return are_passwords_revealed_;
}

BEGIN_METADATA(EditablePasswordCombobox)
END_METADATA

}  // namespace views

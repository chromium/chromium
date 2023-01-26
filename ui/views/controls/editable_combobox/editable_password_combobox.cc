// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_password_combobox.h"

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
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/vector_icons.h"

namespace views {

namespace {

// The eye-styled icon that serves as a button to toggle the password
// visibility.
class Eye : public ToggleImageButton {
 public:
  METADATA_HEADER(Eye);

  constexpr static int kPaddingWidth = 4;

  explicit Eye(PressedCallback callback)
      : ToggleImageButton(std::move(callback)) {
    SetInstallFocusRingOnFocus(true);
    SetRequestFocusOnPress(true);
    SetBorder(CreateEmptyBorder(kPaddingWidth));

    SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
    SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);

    SetImageFromVectorIconWithColorId(this, kEyeIcon, ui::kColorIcon,
                                      ui::kColorIconDisabled);
    SetToggledImageFromVectorIconWithColorId(
        this, kEyeCrossedIcon, ui::kColorIcon, ui::kColorIconDisabled);
  }

  Eye(const Eye&) = delete;
  Eye& operator=(const Eye&) = delete;
  ~Eye() override = default;
};

BEGIN_METADATA(Eye, ToggleImageButton)
END_METADATA

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
    bool display_arrow)
    : EditableCombobox(std::move(combobox_model),
                       /*filter_on_edit=*/false,
                       /*show_on_empty=*/true,
                       text_context,
                       text_style,
                       display_arrow) {
  eye_ = AddControlElement(std::make_unique<Eye>(base::BindRepeating(
      &EditablePasswordCombobox::RequestTogglePasswordVisibility,
      base::Unretained(this))));
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

void EditablePasswordCombobox::SetIsPasswordRevealPermittedCheck(
    IsPasswordRevealPermittedCheck check) {
  reveal_permitted_check_ = std::move(check);
}

void EditablePasswordCombobox::RequestTogglePasswordVisibility() {
  if (!are_passwords_revealed_ && reveal_permitted_check_ &&
      !reveal_permitted_check_.Run()) {
    return;
  }
  RevealPasswords(!are_passwords_revealed_);
}

BEGIN_METADATA(EditablePasswordCombobox, View)
END_METADATA

}  // namespace views

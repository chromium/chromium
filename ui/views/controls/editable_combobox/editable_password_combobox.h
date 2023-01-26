// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_PASSWORD_COMBOBOX_H_
#define UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_PASSWORD_COMBOBOX_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"

namespace views {
class ToggleImageButton;

// Textfield that also shows a drop-down list with suggestions and can switch
// between visible and obfuscated text.
class VIEWS_EXPORT EditablePasswordCombobox : public EditableCombobox {
 public:
  METADATA_HEADER(EditablePasswordCombobox);

  using IsPasswordRevealPermittedCheck = base::RepeatingCallback<bool(void)>;

  static constexpr int kDefaultTextContext = style::CONTEXT_BUTTON;
  static constexpr int kDefaultTextStyle = style::STYLE_PRIMARY;

  EditablePasswordCombobox();

  // `combobox_model`: The ComboboxModel that gives us the items to show in the
  // menu.
  // `text_context` and `text_style`: Together these indicate the font to use.
  // `display_arrow`: Whether to display an arrow in the combobox to indicate
  // that there is a drop-down list.
  explicit EditablePasswordCombobox(
      std::unique_ptr<ui::ComboboxModel> combobox_model,
      int text_context = kDefaultTextContext,
      int text_style = kDefaultTextStyle,
      bool display_arrow = true);

  EditablePasswordCombobox(const EditablePasswordCombobox&) = delete;
  EditablePasswordCombobox& operator=(const EditablePasswordCombobox&) = delete;

  ~EditablePasswordCombobox() override;

  // Sets the tooltips for the password eye icon.
  void SetPasswordIconTooltips(const std::u16string& tooltip_text,
                               const std::u16string& toggled_tooltip_text);

  // Sets and gets whether the textfield and drop-down menu reveal their current
  // content.
  void RevealPasswords(bool revealed);
  bool ArePasswordsRevealed() const;

  // Sets the callback to check whether revealing a password is permitted.
  void SetIsPasswordRevealPermittedCheck(IsPasswordRevealPermittedCheck check);

 private:
  friend class EditablePasswordComboboxTest;

  // Toggles the password visibility. If the password is currently unrevealed,
  // a `PasswordRevealCheck` is set and returns false, then the password remains
  // unrevealed.
  void RequestTogglePasswordVisibility();

  ToggleImageButton* GetEyeButtonForTesting() { return eye_.get(); }

  raw_ptr<ToggleImageButton> eye_ = nullptr;

  // Indicates whether the passwords are currently revealed.
  bool are_passwords_revealed_ = false;

  // A callback to check whether the password is allowed to be revealed.
  IsPasswordRevealPermittedCheck reveal_permitted_check_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_PASSWORD_COMBOBOX_H_

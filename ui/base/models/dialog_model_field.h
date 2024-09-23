// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_DIALOG_MODEL_FIELD_H_
#define UI_BASE_MODELS_DIALOG_MODEL_FIELD_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"

namespace ui {

class DialogModelParagraph;
class DialogModelCheckbox;
class DialogModelCombobox;
class DialogModelCustomField;
class DialogModelMenuItem;
class DialogModelTitleItem;
class DialogModelSection;
class DialogModelTextfield;
class DialogModelPasswordField;
class Event;

class DialogModelFieldHost {
 protected:
  // This PassKey is used to make sure that some methods on DialogModel
  // are only called as part of the host integration.
  static base::PassKey<DialogModelFieldHost> GetPassKey() {
    return base::PassKey<DialogModelFieldHost>();
  }
};

// TODO(pbos): Move this to separate header.
// DialogModelLabel is an exception to below classes. This is not a
// DialogModelField but rather represents a text label and styling. This is used
// with DialogModelParagraph and DialogModelCheckbox for instance and has
// support for styling text replacements and showing a link.
class COMPONENT_EXPORT(UI_BASE) DialogModelLabel {
 public:
  // TODO(pbos): Move this definition (maybe as a ui::LinkCallback) so it can
  // be reused with views::Link.
  using Callback = base::RepeatingCallback<void(const Event& event)>;

  class COMPONENT_EXPORT(UI_BASE) TextReplacement {
   public:
    TextReplacement(const TextReplacement&);
    ~TextReplacement();

    const std::u16string& text() const { return text_; }
    bool is_emphasized() const { return is_emphasized_; }
    const std::optional<Callback>& callback() const { return callback_; }
    const std::optional<std::u16string>& accessible_name() const {
      return accessible_name_;
    }

   private:
    friend class DialogModelLabel;

    // Used for regular and emphasized text.
    explicit TextReplacement(std::u16string text, bool is_emphasized = false);
    // Used for links.
    TextReplacement(int message_id,
                    Callback closure,
                    std::u16string accessible_name = std::u16string());

    const std::u16string text_;
    const bool is_emphasized_;
    const std::optional<Callback> callback_;
    const std::optional<std::u16string> accessible_name_;
  };

  explicit DialogModelLabel(int message_id);
  explicit DialogModelLabel(std::u16string fixed_string);

  DialogModelLabel(const DialogModelLabel&);
  DialogModelLabel& operator=(const DialogModelLabel&) = delete;
  ~DialogModelLabel();

  static DialogModelLabel CreateWithReplacement(int message_id,
                                                TextReplacement replacement);
  static DialogModelLabel CreateWithReplacements(
      int message_id,
      std::vector<TextReplacement> replacements);

  // Builder methods for TextReplacements.
  static TextReplacement CreateLink(
      int message_id,
      base::RepeatingClosure closure,
      std::u16string accessible_name = std::u16string());
  static TextReplacement CreateLink(
      int message_id,
      Callback callback,
      std::u16string accessible_name = std::u16string());
  static TextReplacement CreatePlainText(std::u16string text);
  static TextReplacement CreateEmphasizedText(std::u16string text);

  // Gets the string. Not for use with replacements, in which case the caller
  // must use replacements() and message_id() to construct the final label. This
  // is required to style the final label appropriately and support replacement
  // callbacks. The caller is responsible for checking replacements().empty()
  // before calling this.
  const std::u16string& GetString() const;

  DialogModelLabel& set_is_secondary() {
    is_secondary_ = true;
    return *this;
  }

  DialogModelLabel& set_allow_character_break() {
    allow_character_break_ = true;
    return *this;
  }

  int message_id() const { return message_id_; }
  const std::vector<TextReplacement>& replacements() const {
    return replacements_;
  }

  bool is_secondary() const { return is_secondary_; }
  bool allow_character_break() const { return allow_character_break_; }

 private:
  explicit DialogModelLabel(int message_id,
                            std::vector<TextReplacement> replacements);

  const int message_id_;
  const std::u16string string_;

  // Set of replacements that will be added to `message_id_`.
  const std::vector<TextReplacement> replacements_;

  bool is_secondary_ = false;
  bool allow_character_break_ = false;
};

// These "field" classes represent entries in a DialogModel. They are owned
// by the model and either created through the model or DialogModel::Builder.
// These entries can be referred to by setting the field's ElementIdentifier in
// construction parameters (::Params::SetId()). They can then
// later be acquired through DialogModel::GetFieldByUniqueId() methods.
// These fields own the data corresponding to their field. For instance, the
// text of a textfield in a model is read using DialogModelTextfield::text() and
// stays in sync with the visible dialog (through DialogModelHosts).
class COMPONENT_EXPORT(UI_BASE) DialogModelField {
 public:
  enum Type {
    kParagraph,
    kCheckbox,
    kCombobox,
    kCustom,
    kMenuItem,
    kSeparator,  // TODO(pbos): Remove kSeparator once it can be implied by
                 // having multiple subsequent kSections (3 sections imply 2
                 // separators).
    kSection,
    kTextfield,
    kPasswordField,
    kTitleItem  // TODO(pengchaocai): Remove kTitleItem once DialogModel
                // supports multiple sections.
  };

  class COMPONENT_EXPORT(UI_BASE) Params {
   public:
    Params() = default;
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params() = default;

    Params& SetVisible(bool is_visible) {
      is_visible_ = is_visible;
      return *this;
    }

   private:
    friend class DialogModel;
    friend class DialogModelField;

    bool is_visible_ = true;
  };

  DialogModelField(const DialogModelField&) = delete;
  DialogModelField& operator=(const DialogModelField&) = delete;
  virtual ~DialogModelField();

  [[nodiscard]] base::CallbackListSubscription AddOnFieldChangedCallback(
      base::RepeatingClosure on_field_changed);

  Type type() const { return type_; }

  void SetVisible(bool visible);
  bool is_visible() const { return is_visible_; }

  const base::flat_set<Accelerator>& accelerators() const {
    return accelerators_;
  }
  ElementIdentifier id() const { return id_; }

  DialogModelParagraph* AsParagraph();
  DialogModelCheckbox* AsCheckbox();
  DialogModelCombobox* AsCombobox();
  DialogModelMenuItem* AsMenuItem();
  const DialogModelMenuItem* AsMenuItem() const;
  const DialogModelTitleItem* AsTitleItem() const;
  DialogModelTextfield* AsTextfield();
  DialogModelPasswordField* AsPasswordField();
  DialogModelSection* AsSection();
  DialogModelCustomField* AsCustomField();

 protected:
  DialogModelField(Type type,
                   ElementIdentifier id,
                   base::flat_set<Accelerator> accelerators,
                   const DialogModelField::Params& params);

  void NotifyOnFieldChanged();

 private:
  friend class DialogModel;
  FRIEND_TEST_ALL_PREFIXES(DialogModelButtonTest, UsesParamsUniqueId);

  const Type type_;
  const ElementIdentifier id_;

  const base::flat_set<Accelerator> accelerators_;

  bool is_visible_;

  base::RepeatingClosureList on_field_changed_;
};

// Field class representing a paragraph.
class COMPONENT_EXPORT(UI_BASE) DialogModelParagraph : public DialogModelField {
 public:
  DialogModelParagraph(const DialogModelLabel& label,
                       std::u16string header,
                       ElementIdentifier id);
  DialogModelParagraph(const DialogModelParagraph&) = delete;
  DialogModelParagraph& operator=(const DialogModelParagraph&) = delete;
  ~DialogModelParagraph() override;

  const DialogModelLabel& label() const { return label_; }

  const std::u16string header() const { return header_; }

 private:
  const DialogModelLabel label_;
  const std::u16string header_;
};

// Field class representing a checkbox with descriptive text.
class COMPONENT_EXPORT(UI_BASE) DialogModelCheckbox : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Params : public DialogModelField::Params {
   public:
    Params() = default;
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params() = default;

    Params& SetIsChecked(bool is_checked) {
      is_checked_ = is_checked;
      return *this;
    }

    Params& SetVisible(bool is_visible) {
      DialogModelField::Params::SetVisible(is_visible);
      return *this;
    }

   private:
    friend class DialogModelCheckbox;

    bool is_checked_ = false;
  };

  DialogModelCheckbox(ElementIdentifier id,
                      const DialogModelLabel& label,
                      const Params& params);
  DialogModelCheckbox(const DialogModelCheckbox&) = delete;
  DialogModelCheckbox& operator=(const DialogModelCheckbox&) = delete;
  ~DialogModelCheckbox() override;

  bool is_checked() const { return is_checked_; }

  void OnChecked(base::PassKey<DialogModelFieldHost>, bool is_checked);
  const DialogModelLabel& label() const { return label_; }

 private:
  const DialogModelLabel label_;
  bool is_checked_;
};

// Field class representing a combobox and corresponding label to describe the
// combobox:
//
//     <label>   [combobox]
// Ex: Folder    [My Bookmarks]
class COMPONENT_EXPORT(UI_BASE) DialogModelCombobox : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Params : public DialogModelField::Params {
   public:
    Params();
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params();

    Params& AddAccelerator(Accelerator accelerator);

    Params& SetAccessibleName(std::u16string accessible_name) {
      accessible_name_ = std::move(accessible_name);
      return *this;
    }

    // The combobox callback is invoked when an item has been selected. This
    // nominally happens when selecting an item in the combobox menu. The
    // selection notably does not change by hovering different items in the
    // combobox menu or navigating it with up/down keys as long as the menu is
    // open.
    Params& SetCallback(base::RepeatingClosure callback);

    Params& SetVisible(bool is_visible) {
      DialogModelField::Params::SetVisible(is_visible);
      return *this;
    }

   private:
    friend class DialogModelCombobox;

    ElementIdentifier id_;
    std::u16string accessible_name_;
    base::RepeatingClosure callback_;
    base::flat_set<Accelerator> accelerators_;
  };

  DialogModelCombobox(ElementIdentifier id,
                      std::u16string label,
                      std::unique_ptr<ui::ComboboxModel> combobox_model,
                      const Params& params);
  DialogModelCombobox(const DialogModelCombobox&) = delete;
  DialogModelCombobox& operator=(const DialogModelCombobox&) = delete;
  ~DialogModelCombobox() override;

  size_t selected_index() const { return selected_index_; }
  ui::ComboboxModel* combobox_model() { return combobox_model_.get(); }

  const std::u16string& label() const { return label_; }
  const std::u16string& accessible_name() const { return accessible_name_; }
  void OnSelectedIndexChanged(base::PassKey<DialogModelFieldHost>,
                              size_t selected_index);
  void OnPerformAction(base::PassKey<DialogModelFieldHost>);

 private:
  friend class DialogModel;

  const std::u16string label_;
  const std::u16string accessible_name_;
  size_t selected_index_;
  std::unique_ptr<ui::ComboboxModel> combobox_model_;
  base::RepeatingClosure callback_;
};

// Field class representing a menu item:
//
//     <icon> <label>
// Ex: [icon] Open URL
class COMPONENT_EXPORT(UI_BASE) DialogModelMenuItem : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Params : public DialogModelField::Params {
   public:
    Params();
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params();

    Params& SetIsEnabled(bool is_enabled);
    Params& SetId(ElementIdentifier id);

    Params& SetVisible(bool is_visible) {
      DialogModelField::Params::SetVisible(is_visible);
      return *this;
    }

   private:
    friend class DialogModelMenuItem;

    bool is_enabled_ = true;
    ElementIdentifier id_;
  };

  DialogModelMenuItem(ImageModel icon,
                      std::u16string label,
                      base::RepeatingCallback<void(int)> callback,
                      const Params& params);
  DialogModelMenuItem(const DialogModelMenuItem&) = delete;
  DialogModelMenuItem& operator=(const DialogModelMenuItem&) = delete;
  ~DialogModelMenuItem() override;

  const ImageModel& icon() const { return icon_; }
  const std::u16string& label() const { return label_; }
  bool is_enabled() const { return is_enabled_; }
  void OnActivated(base::PassKey<DialogModelFieldHost>, int event_flags);

 private:
  const ImageModel icon_;
  const std::u16string label_;
  const bool is_enabled_;
  base::RepeatingCallback<void(int)> callback_;
};

// Field class representing a separator.
class COMPONENT_EXPORT(UI_BASE) DialogModelSeparator : public DialogModelField {
 public:
  DialogModelSeparator();
  DialogModelSeparator(const DialogModelSeparator&) = delete;
  DialogModelSeparator& operator=(const DialogModelSeparator&) = delete;
  ~DialogModelSeparator() override;
};

// Field class representing a title.
// TODO(pengchaocai): Remove DialogModelTitleItem once DialogModel supports
// multiple sections and titles live in sections as optional strings.
class COMPONENT_EXPORT(UI_BASE) DialogModelTitleItem : public DialogModelField {
 public:
  explicit DialogModelTitleItem(std::u16string label,
                                ElementIdentifier id = ElementIdentifier());
  DialogModelTitleItem(const DialogModelSeparator&) = delete;
  DialogModelTitleItem& operator=(const DialogModelSeparator&) = delete;
  ~DialogModelTitleItem() override;
  const std::u16string& label() const { return label_; }

 private:
  const std::u16string label_;
};

// Field class representing a textfield and corresponding label to describe the
// textfield:
//
//     <label>   [textfield]
// Ex: Name      [My email]
class COMPONENT_EXPORT(UI_BASE) DialogModelTextfield : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Params : public DialogModelField::Params {
   public:
    Params();
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params();

    Params& AddAccelerator(Accelerator accelerator);

    Params& SetAccessibleName(std::u16string accessible_name) {
      accessible_name_ = std::move(accessible_name);
      return *this;
    }

    Params& SetVisible(bool is_visible) {
      DialogModelField::Params::SetVisible(is_visible);
      return *this;
    }

   private:
    friend class DialogModelTextfield;

    ElementIdentifier id_;
    std::u16string accessible_name_;
    base::flat_set<Accelerator> accelerators_;
  };

  DialogModelTextfield(ElementIdentifier id,
                       std::u16string label,
                       std::u16string text,
                       const Params& params);
  DialogModelTextfield(const DialogModelTextfield&) = delete;
  DialogModelTextfield& operator=(const DialogModelTextfield&) = delete;
  ~DialogModelTextfield() override;

  const std::u16string& text() const { return text_; }

  const std::u16string& label() const { return label_; }
  const std::u16string& accessible_name() const { return accessible_name_; }
  void OnTextChanged(base::PassKey<DialogModelFieldHost>, std::u16string text);

 private:
  friend class DialogModel;

  const std::u16string label_;
  const std::u16string accessible_name_;
  std::u16string text_;
};

// Field class representing a password field and corresponding label to describe
// the password field. The password can be revealed by clicking on the eye icon.
// If the user enters an incorrect password, the field can be invalidated by
// calling `Invalidate()`:
// - the password field is cleared ;
// - the text field is invalidated, causing its outline to be red ;
// - `incorrect_password_text` is shown below the password field.
// The password field becomes valid again automatically when a new character is
// entered.
class COMPONENT_EXPORT(UI_BASE) DialogModelPasswordField
    : public DialogModelField {
 public:
  // using Params = DialogModelField::Params;

  DialogModelPasswordField(ElementIdentifier id,
                           std::u16string label,
                           std::u16string accessible_name,
                           std::u16string incorrect_password_text,
                           const DialogModelPasswordField::Params& params);
  DialogModelPasswordField(const DialogModelPasswordField&) = delete;
  DialogModelPasswordField& operator=(const DialogModelPasswordField&) = delete;
  ~DialogModelPasswordField() override;

  const std::u16string& text() const { return text_; }

  // Clears the password field, and displays `incorrect_password_text()` until
  // the user starts typing again.
  // Typically used when the user clicks the OK button after they are finished
  // typing, and the password is wrong.
  void Invalidate();

  const std::u16string& label() const { return label_; }
  const std::u16string& accessible_name() const { return accessible_name_; }
  const std::u16string& incorrect_password_text() const {
    return incorrect_password_text_;
  }

  void OnTextChanged(base::PassKey<DialogModelFieldHost>, std::u16string text);
  base::CallbackListSubscription AddOnInvalidateCallback(
      base::PassKey<DialogModelFieldHost>,
      base::RepeatingClosure closure);

 private:
  friend class DialogModel;

  const std::u16string label_;
  const std::u16string accessible_name_;
  const std::u16string incorrect_password_text_;
  std::u16string text_;

  base::RepeatingClosureList on_invalidate_closures_;
};

// Field base class representing a "custom" field. Used for instance to inject
// custom Views into dialogs that use DialogModel.
class COMPONENT_EXPORT(UI_BASE) DialogModelCustomField
    : public DialogModelField {
 public:
  // Base class for fields held by DialogModelField. Calling code is responsible
  // for providing the subclass expected by the DialogModelHost used.
  class COMPONENT_EXPORT(UI_BASE) Field {
   public:
    virtual ~Field();
  };

  DialogModelCustomField(ElementIdentifier id,
                         std::unique_ptr<DialogModelCustomField::Field> field);
  DialogModelCustomField(const DialogModelCustomField&) = delete;
  DialogModelCustomField& operator=(const DialogModelCustomField&) = delete;
  ~DialogModelCustomField() override;

  DialogModelCustomField::Field* field() { return field_.get(); }

 private:
  friend class DialogModel;

  std::unique_ptr<DialogModelCustomField::Field> field_;
};

// Field class representing a section. A section is a list of fields which may
// include subsections too (tree structure).
class COMPONENT_EXPORT(UI_BASE) DialogModelSection final
    : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Builder final {
   public:
    Builder();
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder();

    [[nodiscard]] std::unique_ptr<DialogModelSection> Build();

    Builder& AddParagraph(const DialogModelLabel& label,
                          std::u16string header = std::u16string(),
                          ElementIdentifier id = ElementIdentifier()) {
      section_->AddParagraph(label, std::move(header), id);
      return *this;
    }

    Builder& AddCheckbox(ElementIdentifier id,
                         const DialogModelLabel& label,
                         const DialogModelCheckbox::Params& params =
                             DialogModelCheckbox::Params()) {
      section_->AddCheckbox(id, label, params);
      return *this;
    }

    Builder& AddCombobox(ElementIdentifier id,
                         std::u16string label,
                         std::unique_ptr<ui::ComboboxModel> combobox_model,
                         const DialogModelCombobox::Params& params =
                             DialogModelCombobox::Params()) {
      section_->AddCombobox(id, std::move(label), std::move(combobox_model),
                            params);
      return *this;
    }

    Builder& AddMenuItem(ImageModel icon,
                         std::u16string label,
                         base::RepeatingCallback<void(int)> callback,
                         const DialogModelMenuItem::Params& params =
                             DialogModelMenuItem::Params()) {
      section_->AddMenuItem(std::move(icon), std::move(label),
                            std::move(callback), params);
      return *this;
    }

    Builder& AddSeparator() {
      section_->AddSeparator();
      return *this;
    }

    Builder& AddTextfield(ElementIdentifier id,
                          std::u16string label,
                          std::u16string text,
                          const DialogModelTextfield::Params& params =
                              DialogModelTextfield::Params()) {
      section_->AddTextfield(id, std::move(label), std::move(text), params);
      return *this;
    }

    Builder& AddCustomField(
        std::unique_ptr<DialogModelCustomField::Field> field,
        ElementIdentifier id = ElementIdentifier()) {
      section_->AddCustomField(std::move(field), id);
      return *this;
    }

   private:
    std::unique_ptr<DialogModelSection> section_;
  };

  // TODO(pbos): Params may make sense here? An optional title should be here?
  // TODO(pbos): We may also want to add on_field_added as a callback to that
  // Params struct once it exists.
  DialogModelSection();
  DialogModelSection(const DialogModelSection&) = delete;
  DialogModelSection& operator=(const DialogModelSection&) = delete;
  ~DialogModelSection() override;

  [[nodiscard]] base::CallbackListSubscription AddOnFieldAddedCallback(
      base::RepeatingCallback<void(DialogModelField*)> on_field_added);

  [[nodiscard]] base::CallbackListSubscription AddOnFieldChangedCallback(
      base::RepeatingCallback<void(DialogModelField*)> on_field_changed);

  const std::vector<std::unique_ptr<DialogModelField>>& fields() const {
    return fields_;
  }

  DialogModelField* GetFieldByUniqueId(ElementIdentifier id);
  DialogModelCheckbox* GetCheckboxByUniqueId(ElementIdentifier id);
  DialogModelCombobox* GetComboboxByUniqueId(ElementIdentifier id);
  DialogModelTextfield* GetTextfieldByUniqueId(ElementIdentifier id);
  DialogModelPasswordField* GetPasswordFieldByUniqueId(ElementIdentifier id);

  // Adds a paragraph at the end of the section. A paragraph consists of a
  // label and an optional header.
  void AddParagraph(const DialogModelLabel& label,
                    std::u16string header = std::u16string(),
                    ElementIdentifier id = ElementIdentifier());

  // Adds a checkbox ([checkbox] label) at the end of the section.
  void AddCheckbox(ElementIdentifier id,
                   const DialogModelLabel& label,
                   const DialogModelCheckbox::Params& params =
                       DialogModelCheckbox::Params());

  // Adds a labeled combobox (label: [model]) at the end of the section.
  void AddCombobox(ElementIdentifier id,
                   std::u16string label,
                   std::unique_ptr<ui::ComboboxModel> combobox_model,
                   const DialogModelCombobox::Params& params =
                       DialogModelCombobox::Params());

  // Adds a menu item at the end of the section.
  void AddMenuItem(ImageModel icon,
                   std::u16string label,
                   base::RepeatingCallback<void(int)> callback,
                   const DialogModelMenuItem::Params& params =
                       DialogModelMenuItem::Params());

  // Adds a menu item at the end of the section.
  // TODO(pengchaocai): Refactor this method once dialog_model supports multiple
  // DialogModelSection, when the title would be an optional member of `this`
  // and explicitly adding it might not be needed.
  void AddTitleItem(std::u16string label,
                    ElementIdentifier id = ElementIdentifier());

  // Adds a separator at the end of the section.
  void AddSeparator();

  // Adds a labeled textfield (label: [text]) at the end of the section.
  void AddTextfield(ElementIdentifier id,
                    std::u16string label,
                    std::u16string text,
                    const DialogModelTextfield::Params& params =
                        DialogModelTextfield::Params());

  // Adds a labeled password field (label: [password field]) at the end of the
  // section.
  void AddPasswordField(ElementIdentifier id,
                        std::u16string label,
                        std::u16string accessible_text,
                        std::u16string incorrect_password_text,
                        const DialogModelPasswordField::Params& params =
                            DialogModelPasswordField::Params());

  // Adds a custom field at the end of the section. This is used to inject
  // framework-specific custom UI into dialogs that are otherwise constructed as
  // DialogModelBase derivatives.
  void AddCustomField(std::unique_ptr<DialogModelCustomField::Field> field,
                      ElementIdentifier id = ElementIdentifier());

 private:
  void AddField(std::unique_ptr<DialogModelField> field);
  void OnFieldChanged(DialogModelField* field);

  base::RepeatingCallbackList<void(DialogModelField*)> on_field_added_;
  base::RepeatingCallbackList<void(DialogModelField*)> on_field_changed_;

  std::vector<std::unique_ptr<DialogModelField>> fields_;

  std::vector<base::CallbackListSubscription> field_subscriptions_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_DIALOG_MODEL_FIELD_H_

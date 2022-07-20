// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_DIALOG_MODEL_FIELD_H_
#define UI_BASE_MODELS_DIALOG_MODEL_FIELD_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"

namespace ui {

class DialogModel;
class DialogModelButton;
class DialogModelBodyText;
class DialogModelCheckbox;
class DialogModelCombobox;
class DialogModelCustomField;
class DialogModelHost;
class DialogModelMenuItem;
class DialogModelTextfield;
class Event;

// TODO(pbos): Move this to separate header.
// DialogModelLabel is an exception to below classes. This is not a
// DialogModelField but rather represents a text label and styling. This is used
// with DialogModelBodyText and DialogModelCheckbox for instance and has support
// for showing a link.
class COMPONENT_EXPORT(UI_BASE) DialogModelLabel {
 public:
  struct COMPONENT_EXPORT(UI_BASE) Link {
    // TODO(pbos): Move this definition (maybe as a ui::LinkCallback) so it can
    // be reused with views::Link.
    using Callback = base::RepeatingCallback<void(const Event& event)>;

    Link(int message_id,
         Callback callback,
         std::u16string accessible_name = std::u16string());
    Link(int message_id,
         base::RepeatingClosure closure,
         std::u16string accessible_name = std::u16string());
    Link(const Link&);
    ~Link();

    const int message_id;
    const Callback callback;
    const std::u16string accessible_name;
  };

  explicit DialogModelLabel(int message_id);
  explicit DialogModelLabel(std::u16string fixed_string);
  DialogModelLabel(const DialogModelLabel&);
  DialogModelLabel& operator=(const DialogModelLabel&) = delete;
  ~DialogModelLabel();

  static DialogModelLabel CreateWithLink(int message_id, Link link);

  static DialogModelLabel CreateWithLinks(int message_id,
                                          std::vector<Link> links);

  // Gets the string. Not for use with links, in which case the caller must use
  // links() and message_id() to construct the final label. This is required to
  // style the final label appropriately and support link callbacks. The caller
  // is responsible for checking links().empty() before calling this.
  const std::u16string& GetString(base::PassKey<DialogModelHost>) const;

  DialogModelLabel& set_is_secondary() {
    is_secondary_ = true;
    return *this;
  }

  DialogModelLabel& set_allow_character_break() {
    allow_character_break_ = true;
    return *this;
  }

  int message_id(base::PassKey<DialogModelHost>) const { return message_id_; }
  const std::vector<Link> links(base::PassKey<DialogModelHost>) const {
    return links_;
  }
  bool is_secondary(base::PassKey<DialogModelHost>) const {
    return is_secondary_;
  }
  bool allow_character_break(base::PassKey<DialogModelHost>) const {
    return allow_character_break_;
  }

 private:
  explicit DialogModelLabel(int message_id, std::vector<Link> links);

  const int message_id_;
  const std::u16string string_;
  const std::vector<Link> links_;
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
    kButton,
    kBodyText,
    kCheckbox,
    kCombobox,
    kCustom,
    kMenuItem,
    kSeparator,
    kTextfield
  };

  DialogModelField(const DialogModelField&) = delete;
  DialogModelField& operator=(const DialogModelField&) = delete;
  virtual ~DialogModelField();

  // Methods with base::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation.
  Type type(base::PassKey<DialogModelHost>) const { return type_; }
  const base::flat_set<Accelerator>& accelerators(
      base::PassKey<DialogModelHost>) const {
    return accelerators_;
  }
  ElementIdentifier id(base::PassKey<DialogModelHost>) const { return id_; }
  DialogModelButton* AsButton(base::PassKey<DialogModelHost>);
  DialogModelBodyText* AsBodyText(base::PassKey<DialogModelHost>);
  DialogModelCheckbox* AsCheckbox(base::PassKey<DialogModelHost>);
  DialogModelCombobox* AsCombobox(base::PassKey<DialogModelHost>);
  DialogModelMenuItem* AsMenuItem(base::PassKey<DialogModelHost>);
  const DialogModelMenuItem* AsMenuItem(base::PassKey<DialogModelHost>) const;
  DialogModelTextfield* AsTextfield(base::PassKey<DialogModelHost>);
  DialogModelCustomField* AsCustomField(base::PassKey<DialogModelHost>);

 protected:
  // Children of this class need to be constructed through DialogModel to help
  // enforce that they're added to the model.
  DialogModelField(base::PassKey<DialogModel>,
                   DialogModel* model,
                   Type type,
                   ElementIdentifier id,
                   base::flat_set<Accelerator> accelerators);

  DialogModelButton* AsButton();
  DialogModelBodyText* AsBodyText();
  DialogModelCheckbox* AsCheckbox();
  DialogModelCombobox* AsCombobox();
  const DialogModelMenuItem* AsMenuItem() const;
  DialogModelTextfield* AsTextfield();
  DialogModelCustomField* AsCustomField();

 private:
  friend class DialogModel;
  FRIEND_TEST_ALL_PREFIXES(DialogModelButtonTest, UsesParamsUniqueId);

  const raw_ptr<DialogModel> model_;
  const Type type_;
  const ElementIdentifier id_;

  const base::flat_set<Accelerator> accelerators_;
};

// Field class representing a dialog button.
class COMPONENT_EXPORT(UI_BASE) DialogModelButton : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Params {
   public:
    Params();
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params();

    Params& SetId(ElementIdentifier id);

    Params& AddAccelerator(Accelerator accelerator);

   private:
    friend class DialogModelButton;

    ElementIdentifier id_;
    base::flat_set<Accelerator> accelerators_;
  };

  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelButton(base::PassKey<DialogModel> pass_key,
                    DialogModel* model,
                    base::RepeatingCallback<void(const Event&)> callback,
                    std::u16string label,
                    const Params& params);
  DialogModelButton(const DialogModelButton&) = delete;
  DialogModelButton& operator=(const DialogModelButton&) = delete;
  ~DialogModelButton() override;

  // Methods with base::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation.
  const std::u16string& label(base::PassKey<DialogModelHost>) const {
    return label_;
  }
  void OnPressed(base::PassKey<DialogModelHost>, const Event& event);

 private:
  friend class DialogModel;

  const std::u16string label_;
  // The button callback gets called when the button is activated. Whether
  // that happens on key-press, release, etc. is implementation (and platform)
  // dependent.
  base::RepeatingCallback<void(const Event&)> callback_;
};

// Field class representing body text.
class COMPONENT_EXPORT(UI_BASE) DialogModelBodyText : public DialogModelField {
 public:
  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelBodyText(base::PassKey<DialogModel> pass_key,
                      DialogModel* model,
                      const DialogModelLabel& label,
                      ElementIdentifier id);
  DialogModelBodyText(const DialogModelBodyText&) = delete;
  DialogModelBodyText& operator=(const DialogModelBodyText&) = delete;
  ~DialogModelBodyText() override;

  const DialogModelLabel& label(base::PassKey<DialogModelHost>) const {
    return label_;
  }

 private:
  const DialogModelLabel label_;
};

// Field class representing a checkbox with descriptive text.
class COMPONENT_EXPORT(UI_BASE) DialogModelCheckbox : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Params {
   public:
    Params() = default;
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params() = default;

    Params& SetIsChecked(bool is_checked) {
      is_checked_ = is_checked;
      return *this;
    }

   private:
    friend class DialogModelCheckbox;

    bool is_checked_ = false;
  };

  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelCheckbox(base::PassKey<DialogModel> pass_key,
                      DialogModel* model,
                      ElementIdentifier id,
                      const DialogModelLabel& label,
                      const Params& params);
  DialogModelCheckbox(const DialogModelCheckbox&) = delete;
  DialogModelCheckbox& operator=(const DialogModelCheckbox&) = delete;
  ~DialogModelCheckbox() override;

  bool is_checked() const { return is_checked_; }

  void OnChecked(base::PassKey<DialogModelHost>, bool is_checked);
  const DialogModelLabel& label(base::PassKey<DialogModelHost>) const {
    return label_;
  }

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
  class COMPONENT_EXPORT(UI_BASE) Params {
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

   private:
    friend class DialogModelCombobox;

    ElementIdentifier id_;
    std::u16string accessible_name_;
    base::RepeatingClosure callback_;
    base::flat_set<Accelerator> accelerators_;
  };

  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelCombobox(base::PassKey<DialogModel> pass_key,
                      DialogModel* model,
                      ElementIdentifier id,
                      std::u16string label,
                      std::unique_ptr<ui::ComboboxModel> combobox_model,
                      const Params& params);
  DialogModelCombobox(const DialogModelCombobox&) = delete;
  DialogModelCombobox& operator=(const DialogModelCombobox&) = delete;
  ~DialogModelCombobox() override;

  size_t selected_index() const { return selected_index_; }
  ui::ComboboxModel* combobox_model() { return combobox_model_.get(); }

  // Methods with base::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation.
  const std::u16string& label(base::PassKey<DialogModelHost>) const {
    return label_;
  }
  const std::u16string& accessible_name(base::PassKey<DialogModelHost>) const {
    return accessible_name_;
  }
  void OnSelectedIndexChanged(base::PassKey<DialogModelHost>,
                              size_t selected_index);
  void OnPerformAction(base::PassKey<DialogModelHost>);

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
  class COMPONENT_EXPORT(UI_BASE) Params {
   public:
    Params();
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    ~Params();

    Params& set_is_enabled(bool is_enabled) {
      is_enabled_ = is_enabled;
      return *this;
    }

   private:
    friend class DialogModelMenuItem;

    bool is_enabled_ = true;
  };

  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelMenuItem(base::PassKey<DialogModel> pass_key,
                      DialogModel* model,
                      ImageModel icon,
                      std::u16string label,
                      base::RepeatingCallback<void(int)> callback,
                      const Params& params);
  DialogModelMenuItem(const DialogModelMenuItem&) = delete;
  DialogModelMenuItem& operator=(const DialogModelMenuItem&) = delete;
  ~DialogModelMenuItem() override;

  // Methods with base::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation.
  const ImageModel& icon(base::PassKey<DialogModelHost>) const { return icon_; }
  const std::u16string& label(base::PassKey<DialogModelHost>) const {
    return label_;
  }
  bool is_enabled(base::PassKey<DialogModelHost>) const { return is_enabled_; }
  void OnActivated(base::PassKey<DialogModelHost>, int event_flags);

 private:
  const ImageModel icon_;
  const std::u16string label_;
  const bool is_enabled_;
  base::RepeatingCallback<void(int)> callback_;
};

// Field class representing a separator.
class COMPONENT_EXPORT(UI_BASE) DialogModelSeparator : public DialogModelField {
 public:
  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelSeparator(base::PassKey<DialogModel> pass_key, DialogModel* model);
  DialogModelSeparator(const DialogModelSeparator&) = delete;
  DialogModelSeparator& operator=(const DialogModelSeparator&) = delete;
  ~DialogModelSeparator() override;
};

// Field class representing a textfield and corresponding label to describe the
// textfield:
//
//     <label>   [textfield]
// Ex: Name      [My email]
class COMPONENT_EXPORT(UI_BASE) DialogModelTextfield : public DialogModelField {
 public:
  class COMPONENT_EXPORT(UI_BASE) Params {
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

   private:
    friend class DialogModelTextfield;

    ElementIdentifier id_;
    std::u16string accessible_name_;
    base::flat_set<Accelerator> accelerators_;
  };

  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelTextfield(base::PassKey<DialogModel> pass_key,
                       DialogModel* model,
                       ElementIdentifier id,
                       std::u16string label,
                       std::u16string text,
                       const Params& params);
  DialogModelTextfield(const DialogModelTextfield&) = delete;
  DialogModelTextfield& operator=(const DialogModelTextfield&) = delete;
  ~DialogModelTextfield() override;

  const std::u16string& text() const { return text_; }

  // Methods with base::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation.
  const std::u16string& label(base::PassKey<DialogModelHost>) const {
    return label_;
  }
  const std::u16string& accessible_name(base::PassKey<DialogModelHost>) const {
    return accessible_name_;
  }
  void OnTextChanged(base::PassKey<DialogModelHost>, std::u16string text);

 private:
  friend class DialogModel;

  const std::u16string label_;
  const std::u16string accessible_name_;
  std::u16string text_;
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

  // Note that this is constructed through a DialogModel which adds it to model
  // fields.
  DialogModelCustomField(base::PassKey<DialogModel> pass_key,
                         DialogModel* model,
                         ElementIdentifier id,
                         std::unique_ptr<DialogModelCustomField::Field> field);
  DialogModelCustomField(const DialogModelCustomField&) = delete;
  DialogModelCustomField& operator=(const DialogModelCustomField&) = delete;
  ~DialogModelCustomField() override;

  // Methods with base::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation.
  DialogModelCustomField::Field* field(base::PassKey<DialogModelHost>) {
    return field_.get();
  }

 private:
  friend class DialogModel;

  std::unique_ptr<DialogModelCustomField::Field> field_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_DIALOG_MODEL_FIELD_H_

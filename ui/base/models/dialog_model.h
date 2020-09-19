// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_DIALOG_MODEL_H_
#define UI_BASE_MODELS_DIALOG_MODEL_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/strings/string16.h"
#include "base/util/type_safety/pass_key.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_host.h"
#include "ui/base/ui_base_types.h"

namespace ui {

class ComboboxModel;

// Base class for a Delegate associated with (owned by) a model. Provides a link
// from the delegate back to the model it belongs to (through ::dialog_model()),
// from which fields and the DialogModelHost can be accessed.
class COMPONENT_EXPORT(UI_BASE) DialogModelDelegate {
 public:
  DialogModelDelegate() = default;
  DialogModelDelegate(const DialogModelDelegate&) = delete;
  DialogModelDelegate& operator=(const DialogModelDelegate&) = delete;
  virtual ~DialogModelDelegate() = default;

  DialogModel* dialog_model() { return dialog_model_; }

 private:
  friend class DialogModel;
  void set_dialog_model(DialogModel* model) { dialog_model_ = model; }

  DialogModel* dialog_model_ = nullptr;
};

// DialogModel represents a platform-and-toolkit agnostic data + behavior
// portion of a dialog. This contains the semantics of a dialog, whereas
// DialogModelHost implementations (like views::BubbleDialogModelHost) are
// responsible for interfacing with toolkits to display them. This provides a
// separation of concerns where a DialogModel only needs to be concerned with
// what goes into a dialog, not how it shows.
//
// Example usage (with views as an example DialogModelHost implementation). Note
// that visual presentation (except order of elements) is entirely up to
// DialogModelHost, and separate from client code:
//
// constexpr int kNameTextfield = 1;
// class Delegate : public ui::DialogModelDelegate {
//  public:
//   void OnDialogAccepted() {
//     LOG(ERROR) << "Hello "
//                << dialog_model()->GetTextfield(kNameTextfield)->text();
//   }
// };
// auto model_delegate = std::make_unique<Delegate>();
// auto* model_delegate_ptr = model_delegate.get();
//
// auto dialog_model =
//     ui::DialogModel::Builder(std::move(model_delegate))
//         .SetTitle(base::ASCIIToUTF16("Hello, world!"))
//         .AddOkButton(base::BindOnce(&Delegate::OnDialogAccepted,
//                                     base::Unretained(model_delegate_ptr)))
//         .AddTextfield(
//             base::ASCIIToUTF16("Name"), base::string16(),
//             ui::DialogModelTextfield::Params().SetUniqueId(kNameTextfield))
//         .Build();
//
// // DialogModelBase::Host specific. In this example, uses views-specific
// // code to set a view as an anchor.
// auto bubble =
//     std::make_unique<views::BubbleDialogModelHost>(std::move(dialog_model));
// bubble->SetAnchorView(anchor_view);
// views::Widget* const widget =
//     views::BubbleDialogDelegateView::CreateBubble(bubble.release());
// widget->Show();
class COMPONENT_EXPORT(UI_BASE) DialogModel final {
 public:
  // Builder for DialogModel. Used for properties that are either only or
  // commonly const after construction.
  class COMPONENT_EXPORT(UI_BASE) Builder {
   public:
    explicit Builder(std::unique_ptr<DialogModelDelegate> delegate);
    ~Builder();

    std::unique_ptr<DialogModel> Build() WARN_UNUSED_RESULT;

    Builder& SetShowCloseButton(bool show_close_button) {
      model_->show_close_button_ = show_close_button;
      return *this;
    }

    Builder& SetTitle(base::string16 title) {
      model_->title_ = std::move(title);
      return *this;
    }

    // Make screen readers announce the contents of the dialog as it appears.
    // See |ax::mojom::Role::kAlertDialog|.
    Builder& SetIsAlertDialog() {
      model_->is_alert_dialog_ = true;
      return *this;
    }

    // Disables the default behavior that the dialog closes when deactivated.
    Builder& DisableCloseOnDeactivate() {
      model_->close_on_deactivate_ = false;
      return *this;
    }

    // Called when the dialog is explicitly closed (Esc, close-x). Not called
    // during accept/cancel.
    Builder& SetCloseCallback(base::OnceClosure callback) {
      model_->close_callback_ = std::move(callback);
      return *this;
    }

    // TODO(pbos): Clarify and enforce (through tests) that this is called after
    // {accept,cancel,close} callbacks.
    // Unconditionally called when the dialog closes. Called on top of
    // {accept,cancel,close} callbacks.
    Builder& SetWindowClosingCallback(base::OnceClosure callback) {
      model_->window_closing_callback_ = std::move(callback);
      return *this;
    }

    // Adds a dialog button (ok, cancel) to the dialog. The |callback| is called
    // when the dialog is accepted or cancelled, before it closes. Use
    // base::DoNothing() as callback if you want nothing extra to happen as a
    // result, besides the dialog closing.
    // If no |label| is provided, default strings are chosen by the
    // DialogModelHost implementation.
    Builder& AddOkButton(
        base::OnceClosure callback,
        base::string16 label = base::string16(),
        const DialogModelButton::Params& params = DialogModelButton::Params());
    Builder& AddCancelButton(
        base::OnceClosure callback,
        base::string16 label = base::string16(),
        const DialogModelButton::Params& params = DialogModelButton::Params());

    // Use of the extra button in new dialogs are discouraged. If this is deemed
    // necessary please double-check with UX before adding any new dialogs with
    // them.
    Builder& AddDialogExtraButton(
        base::RepeatingCallback<void(const Event&)> callback,
        base::string16 label,
        const DialogModelButton::Params& params = DialogModelButton::Params());

    // Adds body text. See DialogModel::AddBodyText().
    Builder& AddBodyText(const DialogModelLabel& label) {
      model_->AddBodyText(label);
      return *this;
    }

    // Adds a checkbox. See DialogModel::AddCheckbox().
    Builder& AddCheckbox(int unique_id, const DialogModelLabel& label) {
      model_->AddCheckbox(unique_id, label);
      return *this;
    }

    // Adds a combobox. See DialogModel::AddCombobox().
    Builder& AddCombobox(base::string16 label,
                         std::unique_ptr<ui::ComboboxModel> combobox_model,
                         const DialogModelCombobox::Params& params =
                             DialogModelCombobox::Params()) {
      model_->AddCombobox(std::move(label), std::move(combobox_model), params);
      return *this;
    }

    // Adds a textfield. See DialogModel::AddTextfield().
    Builder& AddTextfield(base::string16 label,
                          base::string16 text,
                          const DialogModelTextfield::Params& params =
                              DialogModelTextfield::Params()) {
      model_->AddTextfield(std::move(label), std::move(text), params);
      return *this;
    }

    // Sets which field should be initially focused in the dialog model. Must be
    // called after that field has been added. Can only be called once.
    Builder& SetInitiallyFocusedField(int unique_id);

   private:
    std::unique_ptr<DialogModel> model_;
  };

  DialogModel(util::PassKey<DialogModel::Builder>,
              std::unique_ptr<DialogModelDelegate> delegate);
  ~DialogModel();

  // The host in which this model is hosted. Set by the Host implementation
  // during Host construction where it takes ownership of |this|.
  DialogModelHost* host() { return host_; }

  // Adds body text at the end of the dialog model.
  void AddBodyText(const DialogModelLabel& label);

  // Adds a checkbox ([checkbox] label) at the end of the dialog model.
  void AddCheckbox(int unique_id, const DialogModelLabel& label);

  // Adds a labeled combobox (label: [model]) at the end of the dialog model.
  void AddCombobox(base::string16 label,
                   std::unique_ptr<ui::ComboboxModel> combobox_model,
                   const DialogModelCombobox::Params& params =
                       DialogModelCombobox::Params());

  // Adds a labeled textfield (label: [text]) at the end of the dialog model.
  void AddTextfield(base::string16 label,
                    base::string16 text,
                    const DialogModelTextfield::Params& params =
                        DialogModelTextfield::Params());

  // Check for the existence of a field. Should not be used if the code path
  // expects the |unique_id| to always be present, as GetFieldByUniqueId() and
  // friends will NOTREACHED() if |unique_id| is not present, detecting the bug.
  bool HasField(int unique_id) const;

  // Gets DialogModelFields from their unique identifier. |unique_id| is
  // supplied to the ::Params class during construction. Supplying a |unique_id|
  // not present in the model is a bug, and the methods will NOTREACHED(). If
  // you have unique fields that are conditionally present, see HasField().
  DialogModelField* GetFieldByUniqueId(int unique_id);
  DialogModelCheckbox* GetCheckboxByUniqueId(int unique_id);
  DialogModelCombobox* GetComboboxByUniqueId(int unique_id);
  DialogModelTextfield* GetTextfieldByUniqueId(int unique_id);

  // Methods with util::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation.
  void OnDialogAccepted(util::PassKey<DialogModelHost>);
  void OnDialogCancelled(util::PassKey<DialogModelHost>);
  void OnDialogClosed(util::PassKey<DialogModelHost>);
  void OnWindowClosing(util::PassKey<DialogModelHost>);

  // Called when added to a DialogModelHost.
  void set_host(util::PassKey<DialogModelHost>, DialogModelHost* host) {
    host_ = host;
  }

  bool show_close_button(util::PassKey<DialogModelHost>) const {
    return show_close_button_;
  }

  const base::string16& title(util::PassKey<DialogModelHost>) const {
    return title_;
  }

  base::Optional<int> initially_focused_field(
      util::PassKey<DialogModelHost>) const {
    return initially_focused_field_;
  }

  bool is_alert_dialog(util::PassKey<DialogModelHost>) const {
    return is_alert_dialog_;
  }

  DialogModelButton* ok_button(util::PassKey<DialogModelHost>) {
    return ok_button_.has_value() ? &ok_button_.value() : nullptr;
  }

  DialogModelButton* cancel_button(util::PassKey<DialogModelHost>) {
    return cancel_button_.has_value() ? &cancel_button_.value() : nullptr;
  }

  DialogModelButton* extra_button(util::PassKey<DialogModelHost>) {
    return extra_button_.has_value() ? &extra_button_.value() : nullptr;
  }

  bool close_on_deactivate(util::PassKey<DialogModelHost>) const {
    return close_on_deactivate_;
  }

  // Accessor for ordered fields in the model. This includes DialogButtons even
  // though they should be handled separately (OK button has fixed position in
  // dialog).
  const std::vector<std::unique_ptr<DialogModelField>>& fields(
      util::PassKey<DialogModelHost>) {
    return fields_;
  }

 private:
  util::PassKey<DialogModel> GetPassKey() {
    return util::PassKey<DialogModel>();
  }

  void AddField(std::unique_ptr<DialogModelField> field);

  std::unique_ptr<DialogModelDelegate> delegate_;
  DialogModelHost* host_ = nullptr;

  bool show_close_button_ = false;
  bool close_on_deactivate_ = true;
  base::string16 title_;

  static constexpr int kExtraButtonId = DIALOG_BUTTON_LAST + 1;
  std::vector<std::unique_ptr<DialogModelField>> fields_;
  base::Optional<int> initially_focused_field_;
  bool is_alert_dialog_ = false;

  base::Optional<DialogModelButton> ok_button_;
  base::Optional<DialogModelButton> cancel_button_;
  base::Optional<DialogModelButton> extra_button_;

  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure close_callback_;

  base::OnceClosure window_closing_callback_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_DIALOG_MODEL_H_
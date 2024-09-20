// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_DIALOG_MODEL_H_
#define UI_BASE_MODELS_DIALOG_MODEL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_host.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
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

  raw_ptr<DialogModel> dialog_model_ = nullptr;
};

// DialogModel represents a platform-and-toolkit agnostic data + behavior
// portion of a dialog. This contains the semantics of a dialog, whereas
// DialogModelHost implementations (like views::BubbleDialogModelHost) are
// responsible for interfacing with toolkits to display them. This provides a
// separation of concerns where a DialogModel only needs to be concerned with
// what goes into a dialog, not how it shows.
//
// DialogModel is supported in views, and android. See
// //ui/android/modal_dialog_wrapper.h for limitations of android support.
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
//         .SetTitle(u"Hello, world!")
//         .AddOkButton(base::BindOnce(&Delegate::OnDialogAccepted,
//                                     base::Unretained(model_delegate_ptr)))
//         .AddTextfield(
//             u"Name", std::u16string(),
//             ui::DialogModelTextfield::Params().SetUniqueId(kNameTextfield))
//         .Build();
//
// // DialogModelBase::Host specific. In this example, uses views-specific
// // code to set a view as an anchor.
// auto bubble =
//     std::make_unique<views::BubbleDialogModelHost>(std::move(dialog_model));
// bubble->SetAnchorView(anchor_view);
// views::Widget* const widget =
//     views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
// widget->Show();
class COMPONENT_EXPORT(UI_BASE) DialogModel final {
 public:
  // Field class representing a dialog button.
  // TODO(pbos): Consider separating this from DialogModelField completely. For
  // instance it doesn't have a corresponding DialogModelField::Type.
  class COMPONENT_EXPORT(UI_BASE) Button final : public DialogModelField {
   public:
    class COMPONENT_EXPORT(UI_BASE) Params : public DialogModelField::Params {
     public:
      Params();
      Params(const Params&) = delete;
      Params& operator=(const Params&) = delete;
      ~Params();

      Params& SetId(ElementIdentifier id);
      Params& SetLabel(std::u16string label);
      Params& SetStyle(std::optional<ButtonStyle> style);
      Params& SetEnabled(bool is_enabled);

      Params& AddAccelerator(Accelerator accelerator);

      Params& SetVisible(bool is_visible) {
        DialogModelField::Params::SetVisible(is_visible);
        return *this;
      }

     private:
      friend class DialogModel;
      friend class Button;

      ElementIdentifier id_;
      std::u16string label_;
      std::optional<ButtonStyle> style_;
      bool is_enabled_ = true;
      base::flat_set<Accelerator> accelerators_;
    };

    Button(base::RepeatingCallback<void(const Event&)> callback,
           const Params& params);
    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;
    ~Button() override;

    const std::u16string& label() const { return label_; }
    const std::optional<ButtonStyle> style() const { return style_; }
    bool is_enabled() const { return is_enabled_; }
    void OnPressed(base::PassKey<DialogModelHost>, const Event& event);

   private:
    friend class DialogModel;

    std::u16string label_;
    const std::optional<ButtonStyle> style_;
    bool is_enabled_;
    // The button callback gets called when the button is activated. Whether
    // that happens on key-press, release, etc. is implementation (and platform)
    // dependent.
    base::RepeatingCallback<void(const Event&)> callback_;
  };

  // A variant for button callbacks that allows different behavior to be
  // specified when a button is pressed.
  using ButtonCallbackVariant = absl::variant<
      // This is the default -- no callback action is taken when the button is
      // pressed and the dialog is closed.
      decltype(base::DoNothing()),
      // Called exactly once when the button is pressed. The dialog will be
      // closed after the callback is run.
      base::OnceClosure,
      // Returns whether the dialog should be closed. This will be called each
      // time the button is pressed.
      base::RepeatingCallback<bool()>>;

  // Builder for DialogModel. Used for properties that are either only or
  // commonly const after construction.
  class COMPONENT_EXPORT(UI_BASE) Builder final {
   public:
    // Constructs a Builder for a DialogModel with a DialogModelDelegate whose
    // lifetime (and storage) is tied to the lifetime of the DialogModel.
    explicit Builder(std::unique_ptr<DialogModelDelegate> delegate);

    // Constructs a DialogModel without a DialogModelDelegate (that doesn't
    // require storage tied to the DialogModel). For access to the DialogModel
    // during construction (for use in callbacks), use model().
    Builder();

    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    [[nodiscard]] std::unique_ptr<DialogModel> Build();

    // Gets the DialogModel. Used for setting up callbacks that make use of the
    // model later once it's fully constructed. This is useful for dialogs or
    // callbacks that don't use DialogModelDelegate and don't have direct access
    // to the model through DialogModelDelegate::dialog_model().
    //
    // Note that the DialogModel* returned here is only for registering
    // callbacks with the DialogModel::Builder. These callbacks share lifetimes
    // with the DialogModel so uses of it will not result in use-after-frees.
    DialogModel* model() { return model_.get(); }

    // Overrides the close-x use for the dialog. Should be avoided as the
    // close-x is generally derived from dialog modality. Kept to allow
    // conversion of dialogs that currently do not allow style.
    // TODO(pbos): Propose UX updates to existing dialogs that require this,
    // then remove OverrideShowCloseButton().
    Builder& OverrideShowCloseButton(bool show_close_button) {
      model_->override_show_close_button_ = show_close_button;
      return *this;
    }

    Builder& SetInternalName(std::string internal_name) {
      model_->internal_name_ = std::move(internal_name);
      return *this;
    }

    Builder& SetTitle(std::u16string title) {
      model_->title_ = std::move(title);
      return *this;
    }

    Builder& SetAccessibleTitle(std::u16string accessible_title) {
      model_->accessible_title_ = std::move(accessible_title);
      return *this;
    }

    Builder& SetSubtitle(std::u16string subtitle) {
      model_->subtitle_ = std::move(subtitle);
      return *this;
    }

    Builder& SetBannerImage(ImageModel banner,
                            ImageModel dark_mode_banner = ImageModel()) {
      model_->banner_ = std::move(banner);
      model_->dark_mode_banner_ = std::move(dark_mode_banner);
      return *this;
    }

    Builder& SetIcon(ImageModel icon,
                     ImageModel dark_mode_icon = ImageModel()) {
      model_->icon_ = std::move(icon);
      model_->dark_mode_icon_ = std::move(dark_mode_icon);
      return *this;
    }

    Builder& SetMainImage(ImageModel main_image) {
      model_->main_image_ = std::move(main_image);
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
    Builder& SetCloseActionCallback(base::OnceClosure callback) {
      model_->close_action_callback_ = std::move(callback);
      return *this;
    }

    // TODO(pbos): Clarify and enforce (through tests) that this is called after
    // {accept,cancel,close} callbacks.
    // Unconditionally called when the dialog destroys. Happens after
    // user-action callbacks (accept, cancel, close), or as a result of dialog
    // destruction. The latter can happen without a user action, for instance as
    // a result of the OS destroying a native Widget in which this dialog is
    // hosted.
    Builder& SetDialogDestroyingCallback(base::OnceClosure callback) {
      model_->dialog_destroying_callback_ = std::move(callback);
      return *this;
    }

    // Adds a dialog button (ok, cancel) to the dialog. The |callback| is called
    // when the dialog is accepted or cancelled, before it closes. Use
    // base::DoNothing() as callback if you want nothing extra to happen as a
    // result, besides the dialog closing.
    // If no |label| is provided, default strings are chosen by the
    // DialogModelHost implementation.
    // TODO(pbos): Reconsider this API, a DialogModelHost does not need to use
    // buttons for accepting/cancelling. Also "ok" should be "accept" to be in
    // sync with other APIs?
    Builder& AddOkButton(ButtonCallbackVariant callback,
                         const Button::Params& params = Button::Params());
    Builder& AddCancelButton(ButtonCallbackVariant callback,
                             const Button::Params& params = Button::Params());

    // Use of the extra button in new dialogs are discouraged. If this is deemed
    // necessary please double-check with UX before adding any new dialogs with
    // them. A button label is required to be set in `params`.
    Builder& AddExtraButton(
        base::RepeatingCallback<void(const Event&)> callback,
        const Button::Params& params);

    // Adds an extra link to the dialog.
    Builder& AddExtraLink(DialogModelLabel::TextReplacement link);

    // Adds a paragraph. See DialogModel::AddParagraph().
    Builder& AddParagraph(const DialogModelLabel& label,
                          std::u16string header = std::u16string(),
                          ElementIdentifier id = ElementIdentifier()) {
      model_->AddParagraph(label, header, id);
      return *this;
    }

    // Adds a checkbox. See DialogModel::AddCheckbox().
    Builder& AddCheckbox(ElementIdentifier id,
                         const DialogModelLabel& label,
                         const DialogModelCheckbox::Params& params =
                             DialogModelCheckbox::Params()) {
      model_->AddCheckbox(id, label, params);
      return *this;
    }

    // Adds a combobox. See DialogModel::AddCombobox().
    Builder& AddCombobox(ElementIdentifier id,
                         std::u16string label,
                         std::unique_ptr<ui::ComboboxModel> combobox_model,
                         const DialogModelCombobox::Params& params =
                             DialogModelCombobox::Params()) {
      model_->AddCombobox(id, std::move(label), std::move(combobox_model),
                          params);
      return *this;
    }

    // Adds a menu item. See DialogModel::AddMenuItem().
    Builder& AddMenuItem(ImageModel icon,
                         std::u16string label,
                         base::RepeatingCallback<void(int)> callback,
                         const DialogModelMenuItem::Params& params =
                             DialogModelMenuItem::Params()) {
      model_->AddMenuItem(std::move(icon), std::move(label),
                          std::move(callback), params);
      return *this;
    }

    // Adds a title item. See DialogModel::AddTitleItem().
    // TODO(pengchaocai): Refactor this method once dialog_model supports
    // multiple DialogModelSection, when the title would be an optional member
    // of DialogModelSection.
    Builder& AddTitleItem(std::u16string label,
                          ElementIdentifier id = ElementIdentifier()) {
      model_->AddTitleItem(std::move(label), id);
      return *this;
    }

    // Adds a separator. See DialogModel::AddSeparator().
    Builder& AddSeparator() {
      model_->AddSeparator();
      return *this;
    }

    // Adds a textfield. See DialogModel::AddTextfield().
    Builder& AddTextfield(ElementIdentifier id,
                          std::u16string label,
                          std::u16string text,
                          const DialogModelTextfield::Params& params =
                              DialogModelTextfield::Params()) {
      model_->AddTextfield(id, std::move(label), std::move(text), params);
      return *this;
    }

    // Adds a password field. See DialogModel::AddPasswordField().
    Builder& AddPasswordField(ElementIdentifier id,
                              std::u16string label,
                              std::u16string accessible_text,
                              std::u16string incorrect_password_text,
                              const DialogModelPasswordField::Params& params =
                                  DialogModelPasswordField::Params()) {
      model_->AddPasswordField(id, std::move(label), std::move(accessible_text),
                               std::move(incorrect_password_text), params);
      return *this;
    }

    // Sets the footnote. See DialogModel::SetFootnote().
    Builder& SetFootnote(const DialogModelLabel& label) {
      model_->SetFootnote(label);
      return *this;
    }

    // Adds a custom field. See DialogModel::AddCustomField().
    Builder& AddCustomField(
        std::unique_ptr<DialogModelCustomField::Field> field,
        ElementIdentifier id = ElementIdentifier()) {
      model_->AddCustomField(std::move(field), id);
      return *this;
    }

    // Overrides default button. Can only be called once. The new default button
    // must exist.
    Builder& OverrideDefaultButton(mojom::DialogButton button);

    // Sets which field should be initially focused in the dialog model. Must be
    // called after that field has been added. Can only be called once.
    Builder& SetInitiallyFocusedField(ElementIdentifier id);

   private:
    Builder& AddButtonInternal(ButtonCallbackVariant callback,
                               const Button::Params& params,
                               std::optional<Button>& model_button,
                               ButtonCallbackVariant& model_callback);

    std::unique_ptr<DialogModel> model_;
  };

  DialogModel(base::PassKey<DialogModel::Builder>,
              std::unique_ptr<DialogModelDelegate> delegate);

  DialogModel(const DialogModel&) = delete;
  DialogModel& operator=(const DialogModel&) = delete;

  ~DialogModel();

  // The host in which this model is hosted. Set by the Host implementation
  // during Host construction where it takes ownership of |this|.
  DialogModelHost* host() { return host_; }

  // Adds a paragraph at the end of the dialog model. A paragraph consists of a
  // label and an optional header.
  void AddParagraph(const DialogModelLabel& label,
                    std::u16string header = std::u16string(),
                    ElementIdentifier id = ElementIdentifier()) {
    contents_.AddParagraph(label, std::move(header), id);
  }

  // Adds a checkbox ([checkbox] label) at the end of the dialog model.
  void AddCheckbox(ElementIdentifier id,
                   const DialogModelLabel& label,
                   const DialogModelCheckbox::Params& params =
                       DialogModelCheckbox::Params()) {
    contents_.AddCheckbox(id, std::move(label), params);
  }

  // Adds a labeled combobox (label: [model]) at the end of the dialog model.
  void AddCombobox(ElementIdentifier id,
                   std::u16string label,
                   std::unique_ptr<ui::ComboboxModel> combobox_model,
                   const DialogModelCombobox::Params& params =
                       DialogModelCombobox::Params()) {
    contents_.AddCombobox(id, std::move(label), std::move(combobox_model),
                          params);
  }

  // Adds a menu item at the end of the dialog model.
  void AddMenuItem(ImageModel icon,
                   std::u16string label,
                   base::RepeatingCallback<void(int)> callback,
                   const DialogModelMenuItem::Params& params =
                       DialogModelMenuItem::Params()) {
    contents_.AddMenuItem(std::move(icon), std::move(label),
                          std::move(callback), params);
  }

  // Adds a title item at the end of the dialog model.
  // TODO(pengchaocai): Refactor this method once dialog_model supports multiple
  // DialogModelSection, when the title would be an optional member of
  // DialogModelSection and explicitly adding it might not be needed.
  void AddTitleItem(std::u16string label,
                    ElementIdentifier id = ElementIdentifier()) {
    contents_.AddTitleItem(std::move(label), id);
  }

  // Adds a separator at the end of the dialog model.
  void AddSeparator() { contents_.AddSeparator(); }

  // Adds a labeled textfield (label: [text]) at the end of the dialog model.
  void AddTextfield(ElementIdentifier id,
                    std::u16string label,
                    std::u16string text,
                    const DialogModelTextfield::Params& params =
                        DialogModelTextfield::Params()) {
    contents_.AddTextfield(id, std::move(label), std::move(text), params);
  }

  // Adds a labeled password field at the end of the dialog model.
  void AddPasswordField(ElementIdentifier id,
                        std::u16string label,
                        std::u16string accessible_text,
                        std::u16string incorrect_password_text,
                        const DialogModelPasswordField::Params& params =
                            DialogModelField::Params()) {
    contents_.AddPasswordField(id, std::move(label), std::move(accessible_text),
                               std::move(incorrect_password_text), params);
  }

  // Sets the footnote.
  void SetFootnote(const DialogModelLabel& label) {
    // It's not supported to set a footnote while the dialog is already showing.
    CHECK(!host_);

    footnote_label_.emplace(label);
  }

  // Adds a custom field at the end of the dialog model. This is used to inject
  // framework-specific custom UI into dialogs that are otherwise constructed as
  // DialogModels.
  void AddCustomField(std::unique_ptr<DialogModelCustomField::Field> field,
                      ElementIdentifier id = ElementIdentifier()) {
    contents_.AddCustomField(std::move(field), id);
  }

  // Check for the existence of a field. Should not be used if the code path
  // expects the |unique_id| to always be present, as GetFieldByUniqueId() and
  // friends will NOTREACHED() if |unique_id| is not present, detecting the bug.
  bool HasField(ElementIdentifier id) const;

  // Gets DialogModelFields from their unique identifier. |unique_id| is
  // supplied to the ::Params class during construction. Supplying a |unique_id|
  // not present in the model is a bug, and the methods will NOTREACHED(). If
  // you have unique fields that are conditionally present, see HasField().
  DialogModelField* GetFieldByUniqueId(ElementIdentifier id);
  DialogModelCheckbox* GetCheckboxByUniqueId(ElementIdentifier id) {
    return contents_.GetCheckboxByUniqueId(id);
  }
  DialogModelCombobox* GetComboboxByUniqueId(ElementIdentifier id) {
    return contents_.GetComboboxByUniqueId(id);
  }
  DialogModelTextfield* GetTextfieldByUniqueId(ElementIdentifier id) {
    return contents_.GetTextfieldByUniqueId(id);
  }
  DialogModelPasswordField* GetPasswordFieldByUniqueId(ElementIdentifier id) {
    return contents_.GetPasswordFieldByUniqueId(id);
  }
  Button* GetButtonByUniqueId(ElementIdentifier id);

  // Methods with base::PassKey<DialogModelHost> are only intended to be called
  // by the DialogModelHost implementation. The returned boolean is used to
  // indicate whether the dialog should close as a result of the action.
  bool OnDialogAcceptAction(base::PassKey<DialogModelHost>);
  bool OnDialogCancelAction(base::PassKey<DialogModelHost>);
  void OnDialogCloseAction(base::PassKey<DialogModelHost>);

  void OnDialogDestroying(base::PassKey<DialogModelHost>);

  void SetVisible(ElementIdentifier id, bool visible);

  void SetButtonLabel(Button* button, const std::u16string& label);

  void SetButtonEnabled(Button* button, bool enabled);

  // Called when added to a DialogModelHost.
  void set_host(base::PassKey<DialogModelHost>, DialogModelHost* host) {
    host_ = host;
  }

  const std::optional<bool>& override_show_close_button(
      base::PassKey<DialogModelHost>) const {
    return override_show_close_button_;
  }

  const std::string& internal_name(base::PassKey<DialogModelHost>) const {
    return internal_name_;
  }

  const std::u16string& title(base::PassKey<DialogModelHost>) const {
    return title_;
  }

  const std::u16string& accessible_title(base::PassKey<DialogModelHost>) const {
    return accessible_title_;
  }

  const std::u16string& subtitle(base::PassKey<DialogModelHost>) const {
    return subtitle_;
  }

  const ImageModel& main_image(base::PassKey<DialogModelHost>) const {
    return main_image_;
  }

  const ImageModel& icon(base::PassKey<DialogModelHost>) const { return icon_; }

  const ImageModel& dark_mode_icon(base::PassKey<DialogModelHost>) const {
    return dark_mode_icon_;
  }

  const ImageModel& banner(base::PassKey<DialogModelHost>) const {
    return banner_;
  }

  const ImageModel& dark_mode_banner(base::PassKey<DialogModelHost>) const {
    return dark_mode_banner_;
  }

  const std::optional<mojom::DialogButton>& override_default_button(
      base::PassKey<DialogModelHost>) const {
    return override_default_button_;
  }

  ElementIdentifier initially_focused_field(
      base::PassKey<DialogModelHost>) const {
    return initially_focused_field_;
  }

  bool is_alert_dialog(base::PassKey<DialogModelHost>) const {
    return is_alert_dialog_;
  }

  Button* ok_button(base::PassKey<DialogModelHost>) {
    return ok_button_.has_value() ? &ok_button_.value() : nullptr;
  }

  Button* cancel_button(base::PassKey<DialogModelHost>) {
    return cancel_button_.has_value() ? &cancel_button_.value() : nullptr;
  }

  Button* extra_button(base::PassKey<DialogModelHost>) {
    return extra_button_.has_value() ? &extra_button_.value() : nullptr;
  }

  DialogModelLabel::TextReplacement* extra_link(
      base::PassKey<DialogModelHost>) {
    return extra_link_.has_value() ? &extra_link_.value() : nullptr;
  }

  const std::optional<DialogModelLabel>& footnote_label() const {
    return footnote_label_;
  }

  bool close_on_deactivate(base::PassKey<DialogModelHost>) const {
    return close_on_deactivate_;
  }

  DialogModelSection* contents() { return &contents_; }

  // TODO(pbos): Replace this with a section() or something.
  const std::vector<std::unique_ptr<DialogModelField>>& fields(
      base::PassKey<DialogModelHost>) {
    return contents_.fields();
  }

 private:
  // Runs the appropriate variant of the provided ButtonCallbackVariant and
  // returns whether the dialog should close as a result.
  static bool RunButtonCallback(ButtonCallbackVariant& callback_variant);

  Button* MaybeGetButtonByUniqueId(ElementIdentifier id);

  std::unique_ptr<DialogModelDelegate> delegate_;
  raw_ptr<DialogModelHost> host_ = nullptr;

  std::optional<bool> override_show_close_button_;
  bool close_on_deactivate_ = true;
  std::string internal_name_;
  std::u16string title_;
  std::u16string accessible_title_;
  std::u16string subtitle_;
  ImageModel icon_;
  ImageModel dark_mode_icon_;

  ImageModel main_image_;

  ImageModel banner_;
  ImageModel dark_mode_banner_;

  std::optional<mojom::DialogButton> override_default_button_;
  DialogModelSection contents_;
  ElementIdentifier initially_focused_field_;
  bool is_alert_dialog_ = false;

  std::optional<Button> ok_button_;
  std::optional<Button> cancel_button_;
  std::optional<Button> extra_button_;
  std::optional<DialogModelLabel::TextReplacement> extra_link_;
  std::optional<DialogModelLabel> footnote_label_;

  ButtonCallbackVariant accept_action_callback_;
  ButtonCallbackVariant cancel_action_callback_;
  base::OnceClosure close_action_callback_;

  base::OnceClosure dialog_destroying_callback_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_DIALOG_MODEL_H_

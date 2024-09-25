// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"

namespace ui {

DialogModel::Button::Params::Params() = default;
DialogModel::Button::Params::~Params() = default;

DialogModel::Button::Params& DialogModel::Button::Params::SetId(
    ElementIdentifier id) {
  CHECK(!id_, base::NotFatalUntil::M123);
  CHECK(id, base::NotFatalUntil::M123);
  id_ = id;
  return *this;
}

DialogModel::Button::Params& DialogModel::Button::Params::SetLabel(
    std::u16string label) {
  CHECK(label_.empty(), base::NotFatalUntil::M123);
  CHECK(!label.empty(), base::NotFatalUntil::M123);
  label_ = label;
  return *this;
}

DialogModel::Button::Params& DialogModel::Button::Params::SetStyle(
    std::optional<ButtonStyle> style) {
  CHECK(style_ != style, base::NotFatalUntil::M123);
  style_ = style;
  return *this;
}

DialogModel::Button::Params& DialogModel::Button::Params::SetEnabled(
    bool is_enabled) {
  is_enabled_ = is_enabled;
  return *this;
}

DialogModel::Button::Params& DialogModel::Button::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModel::Button::Button(
    base::RepeatingCallback<void(const Event&)> callback,
    const DialogModel::Button::Params& params)
    : DialogModelField(kCustom, params.id_, params.accelerators_, params),
      label_(params.label_),
      style_(params.style_),
      is_enabled_(params.is_enabled_),
      callback_(std::move(callback)) {
  CHECK(callback_, base::NotFatalUntil::M123);
}

DialogModel::Button::~Button() = default;

void DialogModel::Button::OnPressed(base::PassKey<DialogModelHost>,
                                    const Event& event) {
  callback_.Run(event);
}

DialogModel::Builder::Builder(std::unique_ptr<DialogModelDelegate> delegate)
    : model_(std::make_unique<DialogModel>(base::PassKey<Builder>(),
                                           std::move(delegate))) {}

DialogModel::Builder::Builder() : Builder(nullptr) {}

DialogModel::Builder::~Builder() {
  CHECK(!model_, base::NotFatalUntil::M123) << "Model should've been built.";
}

std::unique_ptr<DialogModel> DialogModel::Builder::Build() {
  CHECK(model_, base::NotFatalUntil::M123);
  return std::move(model_);
}

DialogModel::Builder& DialogModel::Builder::AddOkButton(
    ButtonCallbackVariant callback,
    const DialogModel::Button::Params& params) {
  return AddButtonInternal(std::move(callback), params, model_->ok_button_,
                           model_->accept_action_callback_);
}

DialogModel::Builder& DialogModel::Builder::AddCancelButton(
    ButtonCallbackVariant callback,
    const DialogModel::Button::Params& params) {
  return AddButtonInternal(std::move(callback), params, model_->cancel_button_,
                           model_->cancel_action_callback_);
}

DialogModel::Builder& DialogModel::Builder::AddButtonInternal(
    ButtonCallbackVariant callback,
    const DialogModel::Button::Params& params,
    std::optional<ui::DialogModel::Button>& model_button,
    ButtonCallbackVariant& model_callback) {
  CHECK(params.is_visible_);
  CHECK(!model_button.has_value());
  absl::visit(
      base::Overloaded{
          [](decltype(base::DoNothing())& callback) {
            // Intentional noop
          },
          [](base::RepeatingCallback<bool()>& callback) { CHECK(callback); },
          [](base::OnceClosure& closure) { CHECK(closure); },
      },
      callback);
  model_callback = std::move(callback);
  // NOTREACHED() is used below to make sure this callback isn't used.
  // DialogModelHost should be using OnDialogCanceled() instead.
  model_button.emplace(base::BindRepeating([](const Event&) { NOTREACHED(); }),
                       params);

  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddExtraButton(
    base::RepeatingCallback<void(const Event&)> callback,
    const DialogModel::Button::Params& params) {
  CHECK(params.is_visible_);
  CHECK(!model_->extra_button_, base::NotFatalUntil::M123);
  CHECK(!model_->extra_link_, base::NotFatalUntil::M123);
  // Extra buttons are required to have labels.
  CHECK(!params.label_.empty(), base::NotFatalUntil::M123);
  model_->extra_button_.emplace(std::move(callback), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddExtraLink(
    DialogModelLabel::TextReplacement link) {
  CHECK(!model_->extra_button_, base::NotFatalUntil::M123);
  CHECK(!model_->extra_link_, base::NotFatalUntil::M123);
  model_->extra_link_.emplace(std::move(link));
  return *this;
}

DialogModel::Builder& DialogModel::Builder::OverrideDefaultButton(
    mojom::DialogButton button) {
  // This can only be called once.
  CHECK(!model_->override_default_button_, base::NotFatalUntil::M123);
  // Confirm the button exists.
  switch (button) {
    case mojom::DialogButton::kNone:
      break;
    case mojom::DialogButton::kOk:
      CHECK(model_->ok_button_, base::NotFatalUntil::M123);
      break;
    case mojom::DialogButton::kCancel:
      CHECK(model_->cancel_button_, base::NotFatalUntil::M123);
      break;
  }
  model_->override_default_button_ = button;
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetInitiallyFocusedField(
    ElementIdentifier id) {
  // This must be called with a non-null id
  CHECK(id, base::NotFatalUntil::M123);
  // This can only be called once.
  CHECK(!model_->initially_focused_field_, base::NotFatalUntil::M123);
  model_->initially_focused_field_ = id;
  return *this;
}

DialogModel::DialogModel(base::PassKey<Builder>,
                         std::unique_ptr<DialogModelDelegate> delegate)
    : delegate_(std::move(delegate)) {
  if (delegate_)
    delegate_->set_dialog_model(this);
}

DialogModel::~DialogModel() = default;

bool DialogModel::HasField(ElementIdentifier id) const {
  return base::ranges::any_of(contents_.fields(),
                              [id](auto& field) {
                                // TODO(pbos): This does not
                                // work recursively yet.
                                CHECK_NE(field->type_,
                                         DialogModelField::kSection);
                                return field->id_ == id;
                              }) ||
         (ok_button_ && ok_button_->id_ == id) ||
         (cancel_button_ && cancel_button_->id_ == id) ||
         (extra_button_ && extra_button_->id_ == id);
}

DialogModelField* DialogModel::GetFieldByUniqueId(ElementIdentifier id) {
  // TODO(pbos): Make sure buttons aren't accessed through GetFieldByUniqueId.
  // Then make this simply forward to contents_.
  if (Button* const button = MaybeGetButtonByUniqueId(id)) {
    return button;
  }

  return contents_.GetFieldByUniqueId(id);
}

DialogModel::Button* DialogModel::GetButtonByUniqueId(ElementIdentifier id) {
  Button* const button = MaybeGetButtonByUniqueId(id);
  CHECK(button);
  return button;
}

DialogModel::Button* DialogModel::MaybeGetButtonByUniqueId(
    ElementIdentifier id) {
  if (ok_button_ && ok_button_->id_ == id) {
    return &ok_button_.value();
  }
  if (cancel_button_ && cancel_button_->id_ == id) {
    return &cancel_button_.value();
  }
  if (extra_button_ && extra_button_->id_ == id) {
    return &extra_button_.value();
  }
  return nullptr;
}

bool DialogModel::OnDialogAcceptAction(base::PassKey<DialogModelHost>) {
  return RunButtonCallback(accept_action_callback_);
}

bool DialogModel::OnDialogCancelAction(base::PassKey<DialogModelHost>) {
  return RunButtonCallback(cancel_action_callback_);
}

bool DialogModel::RunButtonCallback(ButtonCallbackVariant& callback_variant) {
  return absl::visit(
      base::Overloaded{
          [](decltype(base::DoNothing())& callback) { return true; },
          [](base::RepeatingCallback<bool()>& callback) {
            return callback.Run();
          },
          [](base::OnceClosure& callback) {
            CHECK(callback);
            std::move(callback).Run();
            return true;
          },
      },
      callback_variant);
}

void DialogModel::OnDialogCloseAction(base::PassKey<DialogModelHost>) {
  if (close_action_callback_)
    std::move(close_action_callback_).Run();
}

void DialogModel::OnDialogDestroying(base::PassKey<DialogModelHost>) {
  if (dialog_destroying_callback_)
    std::move(dialog_destroying_callback_).Run();
}

void DialogModel::SetVisible(ElementIdentifier id, bool visible) {
  // TODO(pbos): Consider a different method for dialog buttons vs. contents.
  if (Button* button = MaybeGetButtonByUniqueId(id)) {
    button->SetVisible(visible);
    if (host_) {
      host_->OnDialogButtonChanged();
    }
    return;
  }

  GetFieldByUniqueId(id)->SetVisible(visible);
}

void DialogModel::SetButtonLabel(DialogModel::Button* button,
                                 const std::u16string& label) {
  CHECK(button);
  button->label_ = label;

  if (host_) {
    host_->OnDialogButtonChanged();
  }
}

void DialogModel::SetButtonEnabled(DialogModel::Button* button, bool enabled) {
  CHECK(button);
  button->is_enabled_ = enabled;

  if (host_) {
    host_->OnDialogButtonChanged();
  }
}

}  // namespace ui

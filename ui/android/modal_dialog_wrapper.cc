// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/modal_dialog_wrapper.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/strings/grit/ui_strings.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/ModalDialogWrapper_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace ui {

ModalDialogWrapper* g_dialog_ptr_for_testing = nullptr;

// static:
void ModalDialogWrapper::ShowTabModal(
    std::unique_ptr<ui::DialogModel> dialog_model,
    ui::WindowAndroid* window) {
  ModalDialogWrapper* tab_modal =
      new ModalDialogWrapper(std::move(dialog_model), window);
  g_dialog_ptr_for_testing = tab_modal;
  tab_modal->BuildPropertyModel();
  auto* dialog_manager = window->GetModalDialogManagerBridge();
  CHECK(dialog_manager);
  dialog_manager->ShowDialog(tab_modal->java_obj_,
                             ModalDialogManagerBridge::ModalDialogType::kTab);
  // `tab_modal` will delete itself when dialog is dismissed.
}

// static:
ModalDialogWrapper* ModalDialogWrapper::GetDialogForTesting() {
  return g_dialog_ptr_for_testing;
}

// private:
ModalDialogWrapper::ModalDialogWrapper(
    std::unique_ptr<DialogModel> dialog_model,
    ui::WindowAndroid* window_android)
    : dialog_model_(std::move(dialog_model)), window_android_(window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_ = Java_ModalDialogWrapper_create(
      env, reinterpret_cast<uintptr_t>(this), window_android_->GetJavaObject());
}

ModalDialogWrapper::~ModalDialogWrapper() {
  dialog_model_->OnDialogDestroying(DialogModelHost::GetPassKey());
}

namespace {  // private helper for ModalDialogWrapper::BuildPropertyModel

ScopedJavaLocalRef<jstring> GetButtonLabel(JNIEnv* env,
                                           DialogModel::Button* button,
                                           int default_label_id) {
  if (!button) {
    return ScopedJavaLocalRef<jstring>();
  }
  const std::u16string& label_text = button->label();
  return ConvertUTF16ToJavaString(
      env, label_text.empty() ? l10n_util::GetStringUTF16(default_label_id)
                              : label_text);
}

std::u16string getMessageParagraph(DialogModelField* field) {
  const DialogModelLabel& label = field->AsParagraph()->label();

  std::u16string text;
  auto replacements = label.replacements();
  if (replacements.empty()) {
    text = label.GetString();
  } else {
    std::vector<std::u16string> string_replacements;
    for (auto replacement : replacements) {
      string_replacements.push_back(replacement.text());
    }
    text = l10n_util::GetStringFUTF16(label.message_id(), string_replacements,
                                      nullptr);
  }
  return text;
}

}  // anonymous namespace

ModalDialogWrapper::ModalDialogButtonStyles
ModalDialogWrapper::GetButtonStyles() const {
  auto* ok_button = dialog_model_->ok_button(DialogModelHost::GetPassKey());
  if (!ok_button) {
    return ModalDialogButtonStyles::kPrimaryOutlineNegativeOutline;
  }

  auto* cancel_button =
      dialog_model_->cancel_button(DialogModelHost::GetPassKey());

  const ButtonStyle ok_button_style =
      ok_button->style().value_or(ButtonStyle::kDefault);
  const ButtonStyle cancel_button_style =
      cancel_button ? cancel_button->style().value_or(ui::ButtonStyle::kDefault)
                    : ButtonStyle::kDefault;

  const std::optional<mojom::DialogButton>& override_default_button =
      dialog_model_->override_default_button(DialogModelHost::GetPassKey());

  const bool is_ok_prominent =
      (override_default_button == mojom::DialogButton::kOk) ||
      (ok_button_style == ui::ButtonStyle::kProminent &&
       !override_default_button.has_value());

  const bool is_cancel_prominent =
      (override_default_button == mojom::DialogButton::kCancel) ||
      (cancel_button_style == ui::ButtonStyle::kProminent &&
       !override_default_button.has_value());

  if (is_ok_prominent && is_cancel_prominent) {
    NOTREACHED() << "Both buttons cannot be prominent.";
  }

  if (is_ok_prominent) {
    return (cancel_button)
               ? ModalDialogButtonStyles::kPrimaryFilledNegativeOutline
               : ModalDialogButtonStyles::kPrimaryFilledNoNegative;
  }

  if (is_cancel_prominent) {
    return ModalDialogButtonStyles::kPrimaryOutlineNegativeFilled;
  }

  return ModalDialogButtonStyles::kPrimaryOutlineNegativeOutline;
}

void ModalDialogWrapper::BuildPropertyModel() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> title = ConvertUTF16ToJavaString(
      env, dialog_model_->title(DialogModelHost::GetPassKey()));

  ScopedJavaLocalRef<jstring> ok_button_label = GetButtonLabel(
      env, dialog_model_->ok_button(DialogModelHost::GetPassKey()), IDS_APP_OK);
  ScopedJavaLocalRef<jstring> cancel_button_label = GetButtonLabel(
      env, dialog_model_->cancel_button(DialogModelHost::GetPassKey()),
      IDS_APP_CANCEL);

  ModalDialogButtonStyles buttonStyles = GetButtonStyles();

  Java_ModalDialogWrapper_withTitleAndButtons(
      env, java_obj_, title, ok_button_label, cancel_button_label,
      (int)buttonStyles);

  std::vector<std::u16string> paragraphs;
  for (const auto& field :
       dialog_model_->fields(DialogModelHost::GetPassKey())) {
    switch (field->type()) {
      case DialogModelField::kParagraph: {
        paragraphs.push_back(getMessageParagraph(field.get()));
        break;
      }
      default:
        NOTREACHED() << "Unsupported DialogModel field type " << field->type()
                     << ". Support should be added before this dialog is used "
                        "in android";
    }
  }

  if (paragraphs.size() > 0) {
    ScopedJavaLocalRef<jobjectArray> java_paragraphs_array =
        base::android::ToJavaArrayOfStrings(env, paragraphs);

    Java_ModalDialogWrapper_withMessageParagraphs(env, java_obj_,
                                                  java_paragraphs_array);
  }
}

void ModalDialogWrapper::PositiveButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogAcceptAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::NegativeButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogCancelAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::Dismissed(JNIEnv* env) {
  dialog_model_->OnDialogCloseAction(DialogModelHost::GetPassKey());
}

void ModalDialogWrapper::Destroy(JNIEnv* env) {
  delete this;
}

void ModalDialogWrapper::Close() {
  auto* dialog_manager = window_android_->GetModalDialogManagerBridge();
  CHECK(dialog_manager) << "The destruction of the ModalDialogManager.java "
                           "should also destroy this dialog wrapper.";
  dialog_manager->DismissDialog(java_obj_);
}

void ModalDialogWrapper::OnDialogButtonChanged() {}

}  // namespace ui

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

ModalDialogWrapper::~ModalDialogWrapper() = default;

void ModalDialogWrapper::BuildPropertyModel() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> title = ConvertUTF16ToJavaString(
      env, dialog_model_->title(DialogModelHost::GetPassKey()));
  auto* ok_button = dialog_model_->ok_button(DialogModelHost::GetPassKey());
  ScopedJavaLocalRef<jstring> ok_button_label;
  if (ok_button) {
    ok_button_label = ConvertUTF16ToJavaString(
        env, ok_button->label().empty() ? l10n_util::GetStringUTF16(IDS_APP_OK)
                                        : ok_button->label());
  }
  auto* cancel_button =
      dialog_model_->cancel_button(DialogModelHost::GetPassKey());
  ScopedJavaLocalRef<jstring> cancel_button_label;
  if (cancel_button) {
    cancel_button_label = ConvertUTF16ToJavaString(
        env, cancel_button->label().empty()
                 ? l10n_util::GetStringUTF16(IDS_APP_CANCEL)
                 : cancel_button->label());
  }

  Java_ModalDialogWrapper_withTitleAndButtons(
      env, java_obj_, title, ok_button_label, cancel_button_label);

  int paragraph_count = 0;
  for (const auto& field :
       dialog_model_->fields(DialogModelHost::GetPassKey())) {
    switch (field->type()) {
      case DialogModelField::kParagraph: {
        // TODO(joelhockey): Add multi-paragraph support - clank supports 2.
        CHECK_EQ(++paragraph_count, 1) << "Only single paragraph supported. "
                                          "Fix me if you need multi-paragraph,";
        std::u16string text;
        const DialogModelLabel& label = field->AsParagraph()->label();
        auto replacements = label.replacements();
        if (replacements.empty()) {
          text = label.GetString();
        } else {
          std::vector<std::u16string> string_replacements;
          for (auto replacement : replacements) {
            string_replacements.push_back(replacement.text());
          }
          text = l10n_util::GetStringFUTF16(label.message_id(),
                                            string_replacements, nullptr);
        }
        Java_ModalDialogWrapper_withParagraph1(
            env, java_obj_, ConvertUTF16ToJavaString(env, text));
        break;
      }
      default:
        NOTREACHED() << "Unsupported DialogModel field type " << field->type()
                     << ". Support should be added before this dialog is used "
                        "in android";
    }
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

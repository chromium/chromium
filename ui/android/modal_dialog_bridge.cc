// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/modal_dialog_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/strings/grit/ui_strings.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/ModalDialogBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace ui {
namespace {
static const int kDialogDismissalCause_DISMISSED_BY_NATIVE = 4;
}

// static
void ModalDialogBridge::ShowTabModal(
    std::unique_ptr<ui::DialogModel> dialog_model,
    ui::WindowAndroid* web_contents) {
  ModalDialogBridge* delegate = new ModalDialogBridge(std::move(dialog_model));
  delegate->BuildPropertyModel();
  delegate->Show(web_contents);
  // delegate will delete itself when dialog is dismissed.
}

ModalDialogBridge::ModalDialogBridge(std::unique_ptr<DialogModel> dialog_model)
    : dialog_model_(std::move(dialog_model)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_ =
      Java_ModalDialogBridge_create(env, reinterpret_cast<uintptr_t>(this));
}

ModalDialogBridge::~ModalDialogBridge() = default;

void ModalDialogBridge::BuildPropertyModel() {
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

  Java_ModalDialogBridge_withTitleAndButtons(
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
        Java_ModalDialogBridge_withParagraph1(
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

void ModalDialogBridge::Show(ui::WindowAndroid* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ModalDialogBridge_showTabModal(env, java_obj_,
                                      web_contents->GetJavaObject());
}

void ModalDialogBridge::Dismiss(int dismissalCause) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ModalDialogBridge_onDismiss(env, java_obj_, nullptr, dismissalCause);
}

void ModalDialogBridge::PositiveButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogAcceptAction(DialogModelHost::GetPassKey());
}

void ModalDialogBridge::NegativeButtonClicked(JNIEnv* env) {
  dialog_model_->OnDialogCancelAction(DialogModelHost::GetPassKey());
}

void ModalDialogBridge::Dismissed(JNIEnv* env) {
  dialog_model_->OnDialogCloseAction(DialogModelHost::GetPassKey());
}

void ModalDialogBridge::Destroy(JNIEnv* env) {
  delete this;
}

void ModalDialogBridge::Close() {
  Dismiss(kDialogDismissalCause_DISMISSED_BY_NATIVE);
}

void ModalDialogBridge::OnDialogButtonChanged() {}

}  // namespace ui

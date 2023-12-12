// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/dialogs/user_dialog_manager_bridge.h"

#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/browser_thread.h"
#include "wolvic/jni_headers/UserDialogManagerBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace wolvic {

struct InProgressDialog {
  explicit InProgressDialog(DialogCallback callback);
  InProgressDialog(const InProgressDialog&) = delete;
  InProgressDialog& operator=(const InProgressDialog&) = delete;
  ~InProgressDialog();

  DialogCallback callback;
};

InProgressDialog::InProgressDialog(DialogCallback callback)
    : callback(std::move(callback)) {}

InProgressDialog::~InProgressDialog() = default;

UserDialogManagerBridge::UserDialogManagerBridge() = default;

UserDialogManagerBridge::~UserDialogManagerBridge() = default;

UserDialogManagerBridge* UserDialogManagerBridge::GetInstance() {
  static base::NoDestructor<UserDialogManagerBridge> instance;
  return instance.get();
}

void UserDialogManagerBridge::ShowAlertDialog(const std::u16string& message,
                                              DialogCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  auto java_message =
      ScopedJavaGlobalRef(ConvertUTF16ToJavaString(env, message));
  in_progress_dialogs_.emplace_back(
      std::make_unique<InProgressDialog>(std::move(callback)));
  Java_UserDialogManagerBridge_onAlertDialog(
      env, java_message,
      reinterpret_cast<jlong>(in_progress_dialogs_.back().get()));
}

void UserDialogManagerBridge::ShowConfirmDialog(const std::u16string& message,
                                                DialogCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  auto java_message =
      ScopedJavaGlobalRef(ConvertUTF16ToJavaString(env, message));
  in_progress_dialogs_.emplace_back(
      std::make_unique<InProgressDialog>(std::move(callback)));
  Java_UserDialogManagerBridge_onConfirmDialog(
      env, java_message,
      reinterpret_cast<jlong>(in_progress_dialogs_.back().get()));
}

void UserDialogManagerBridge::ShowTextDialog(
    const std::u16string& message,
    const std::u16string& default_user_input,
    DialogCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  auto java_message =
      ScopedJavaGlobalRef(ConvertUTF16ToJavaString(env, message));
  auto java_default_user_input =
      ScopedJavaGlobalRef(ConvertUTF16ToJavaString(env, default_user_input));
  in_progress_dialogs_.emplace_back(
      std::make_unique<InProgressDialog>(std::move(callback)));
  Java_UserDialogManagerBridge_onTextDialog(
      env, java_message, java_default_user_input,
      reinterpret_cast<jlong>(in_progress_dialogs_.back().get()));
}

void UserDialogManagerBridge::ShowBeforeUnloadDialog(DialogCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  in_progress_dialogs_.emplace_back(
      std::make_unique<InProgressDialog>(std::move(callback)));
  Java_UserDialogManagerBridge_onBeforeUnloadDialog(
      env, reinterpret_cast<jlong>(in_progress_dialogs_.back().get()));
}

void UserDialogManagerBridge::OnDialogClosed(InProgressDialog* dialog,
                                             DialogResult result,
                                             const std::u16string& user_input) {
  auto it =
      std::find_if(in_progress_dialogs_.begin(), in_progress_dialogs_.end(),
                   [dialog](const auto& p) { return p.get() == dialog; });
  CHECK(it != in_progress_dialogs_.end());
  std::move((*it)->callback)
      .Run(result == DialogResult::kConfirmed, user_input);
  in_progress_dialogs_.erase(it);
}

void JNI_UserDialogManagerBridge_ConfirmDialog(
    JNIEnv* env,
    jlong in_progress_dialog_ptr,
    const JavaParamRef<jstring>& java_user_input) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* dialog = reinterpret_cast<InProgressDialog*>(in_progress_dialog_ptr);
  std::u16string user_input;
  if (!java_user_input.is_null()) {
    user_input = ConvertJavaStringToUTF16(env, java_user_input);
  }
  UserDialogManagerBridge::GetInstance()->OnDialogClosed(
      dialog, DialogResult::kConfirmed, user_input);
}

static void JNI_UserDialogManagerBridge_DismissDialog(
    JNIEnv* env,
    jlong in_progress_dialog_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* dialog = reinterpret_cast<InProgressDialog*>(in_progress_dialog_ptr);
  UserDialogManagerBridge::GetInstance()->OnDialogClosed(
      dialog, DialogResult::kDismissed, std::u16string());
}

}  // namespace wolvic

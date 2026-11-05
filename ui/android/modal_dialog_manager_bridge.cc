// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/modal_dialog_manager_bridge.h"

#include "base/android/jni_android.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/ModalDialogManagerBridge_jni.h"

namespace ui {

ModalDialogManagerBridge::ModalDialogManagerBridge(
    const jni_zero::JavaRef<jobject>& manager)
    : j_modal_dialog_manager_bridge_(manager) {}

ModalDialogManagerBridge::~ModalDialogManagerBridge() = default;

int ModalDialogManagerBridge::SuspendModalDialog(ModalDialogType dialog_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ModalDialogManagerBridge_suspendModalDialogs(
      env, j_modal_dialog_manager_bridge_, static_cast<int>(dialog_type));
}

void ModalDialogManagerBridge::ResumeModalDialog(ModalDialogType dialog_type,
                                                 int token) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ModalDialogManagerBridge_resumeModalDialogs(
      env, j_modal_dialog_manager_bridge_, static_cast<int>(dialog_type),
      token);
}

void ModalDialogManagerBridge::ShowDialog(
    const jni_zero::JavaRef<jobject>& j_modal_dialog_wrapper,
    ModalDialogType dialog_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ModalDialogManagerBridge_showDialog(env, j_modal_dialog_manager_bridge_,
                                           j_modal_dialog_wrapper,
                                           static_cast<int>(dialog_type));
}

void ModalDialogManagerBridge::DismissDialog(
    const jni_zero::JavaRef<jobject>& j_modal_dialog_wrapper) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ModalDialogManagerBridge_dismissDialog(
      env, j_modal_dialog_manager_bridge_, j_modal_dialog_wrapper);
}

// `ui/android/ui_android_jni_headers/ModalDialogManagerBridge_jni.h`
// implementations.

static auto JNI_ModalDialogManagerBridge_Create(
    const jni_zero::JavaRef<jobject>& manager) {
  return jni_zero::MakeUnique<ModalDialogManagerBridge>(manager);
}

}  // namespace ui

DEFINE_JNI(ModalDialogManagerBridge)

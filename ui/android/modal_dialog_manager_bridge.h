// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_MODAL_DIALOG_MANAGER_BRIDGE_H_
#define UI_ANDROID_MODAL_DIALOG_MANAGER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "ui/android/ui_android_export.h"

namespace ui {

// The JNI bridge for the Java's `ModalDialogManager`. This bridge's lifetime is
// managed by the Java's `ModalDialogManager`.
class UI_ANDROID_EXPORT ModalDialogManagerBridge {
 public:
  // Java's `TokenHolder#INVALID_TOKEN`.
  constexpr static int kInvalidDialogToken = -1;

  // Mirrors Java's `ModalDialogManager#ModalDialogType`.
  // TODO(crbug.com/365081468): IntDef that enum in C++.
  enum class ModalDialogType {
    kTab = 0,
    kApp,
  };

  explicit ModalDialogManagerBridge(
      const jni_zero::JavaParamRef<jobject>& manager);
  ~ModalDialogManagerBridge();

  // Suspend / resume all dialogs of `dialog_type`, for the current Window.
  int SuspendModalDialog(ModalDialogType dialog_type);
  void ResumeModalDialog(ModalDialogType dialog_type, int token);

  void ShowDialog(
      base::android::ScopedJavaGlobalRef<jobject> j_modal_dialog_wrapper,
      ModalDialogType dialog_type);
  void DismissDialog(
      base::android::ScopedJavaGlobalRef<jobject> j_modal_dialog_wrapper);

 private:
  // The java's `ModalDialogManagerBridge`.
  base::android::ScopedJavaGlobalRef<jobject> j_modal_dialog_manager_bridge_;
};
}  // namespace ui

#endif  // UI_ANDROID_MODAL_DIALOG_MANAGER_BRIDGE_H_

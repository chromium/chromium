// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_MODAL_DIALOG_BRIDGE_H_
#define UI_ANDROID_MODAL_DIALOG_BRIDGE_H_

#include <memory>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "ui/android/ui_android_export.h"
#include "ui/base/models/dialog_model_host.h"

namespace ui {
class DialogModel;
class WindowAndroid;
}  // namespace ui

namespace ui {

// Allows dialogs defined by ui::DialogModel() to be shown in android.
// This makes a best effort to map between ui::DialogModel() and Java
// PropertyModel.
//
// Maps title, an optional single paragraph, and ok and cancel buttons.
// Default labels for IDS_APP_OK and IDS_APP_CANCEL will be used for buttons
// if labels are not specified.
// Replacements (if any) are performed in paragraph text, but any emphasis is
// not included since it is not supported in android dialogs.
class UI_ANDROID_EXPORT ModalDialogBridge : public ui::DialogModelHost {
 public:
  // Shows a tab modal dialog based on `dialog_model`.
  static void ShowTabModal(std::unique_ptr<ui::DialogModel> dialog_model,
                           ui::WindowAndroid* web_contents);

  explicit ModalDialogBridge(std::unique_ptr<ui::DialogModel> dialog_model);
  ModalDialogBridge(const ModalDialogBridge&) = delete;
  ModalDialogBridge& operator=(const ModalDialogBridge&) = delete;
  virtual ~ModalDialogBridge();

  // JNI methods.
  void PositiveButtonClicked(JNIEnv* env);
  void NegativeButtonClicked(JNIEnv* env);
  void Dismissed(JNIEnv* env);
  void Destroy(JNIEnv* env);

 private:
  // ui::DialogModelHost:
  void Close() override;
  void OnDialogButtonChanged() override;

  // Build java PropertyModel from ui::DialogModel.
  void BuildPropertyModel();

  // Show the dialog. This object will destroy itself when it is dismissed.
  void Show(ui::WindowAndroid* web_contents);

  // Call to java ModalDialogBridge#onDismiss(DialogDismissalCause);
  void Dismiss(int dismissalCause);

  std::unique_ptr<ui::DialogModel> dialog_model_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace ui

#endif  // UI_ANDROID_MODAL_DIALOG_BRIDGE_H_

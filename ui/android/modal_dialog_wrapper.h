// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_MODAL_DIALOG_WRAPPER_H_
#define UI_ANDROID_MODAL_DIALOG_WRAPPER_H_

#include <memory>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
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
class UI_ANDROID_EXPORT ModalDialogWrapper : public DialogModelHost {
 public:
  static void ShowTabModal(std::unique_ptr<ui::DialogModel> dialog_model,
                           ui::WindowAndroid* window);

  static ModalDialogWrapper* GetDialogForTesting();

  ModalDialogWrapper(const ModalDialogWrapper&) = delete;
  ModalDialogWrapper& operator=(const ModalDialogWrapper&) = delete;
  virtual ~ModalDialogWrapper();

  // JNI methods.
  void PositiveButtonClicked(JNIEnv* env);
  void NegativeButtonClicked(JNIEnv* env);
  void Dismissed(JNIEnv* env);
  void Destroy(JNIEnv* env);

 private:
  FRIEND_TEST_ALL_PREFIXES(ModalDialogWrapperTest, CloseDialogFromNative);

  ModalDialogWrapper(std::unique_ptr<ui::DialogModel> dialog_model,
                     ui::WindowAndroid* window_android);

  // `DialogModelHost`:
  void Close() override;
  void OnDialogButtonChanged() override;

  // Build java PropertyModel from ui::DialogModel.
  void BuildPropertyModel();

  const std::unique_ptr<ui::DialogModel> dialog_model_;

  const raw_ptr<WindowAndroid> window_android_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace ui

#endif  // UI_ANDROID_MODAL_DIALOG_WRAPPER_H_

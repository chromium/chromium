// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_ANDROID_SELECT_FILE_DIALOG_ANDROID_H_
#define UI_SHELL_DIALOGS_ANDROID_SELECT_FILE_DIALOG_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ui {

class SelectFileDialogImpl : public SelectFileDialog {
 public:
  static SelectFileDialogImpl* Create(Listener* listener,
                                      std::unique_ptr<SelectFilePolicy> policy);

  void OnFileSelected(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& java_object,
                      const base::android::JavaParamRef<jstring>& filepath,
                      const base::android::JavaParamRef<jstring>& display_name);

  void OnMultipleFilesSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      const base::android::JavaParamRef<jobjectArray>& filepaths,
      const base::android::JavaParamRef<jobjectArray>& display_names);

  void OnFileNotSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object);

  void OnContactsSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      const base::android::JavaParamRef<jstring>& contacts);

  // From SelectFileDialog
  bool IsRunning(gfx::NativeWindow) const override;
  void ListenerDestroyed() override;

  // Called when it is time to display the file picker.
  // params is expected to be a vector<string16> with accept_types first and
  // the capture value as the last element of the vector.
  void SelectFileImpl(SelectFileDialog::Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const SelectFileDialog::FileTypeInfo* file_types,
                      int file_type_index,
                      const std::string& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override;

 protected:
  ~SelectFileDialogImpl() override;

 private:
  SelectFileDialogImpl(Listener* listener,
                       std::unique_ptr<SelectFilePolicy> policy);

  bool HasMultipleFileTypeChoicesImpl() override;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_ANDROID_SELECT_FILE_DIALOG_ANDROID_H_


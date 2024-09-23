// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_ANDROID_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ui {

class SelectFileDialogImpl : public SelectFileDialog {
 public:
  static SelectFileDialogImpl* Create(Listener* listener,
                                      std::unique_ptr<SelectFilePolicy> policy);

  SelectFileDialogImpl(const SelectFileDialogImpl&) = delete;
  SelectFileDialogImpl& operator=(const SelectFileDialogImpl&) = delete;

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

  // From SelectFileDialog
  bool IsRunning(gfx::NativeWindow) const override;
  void ListenerDestroyed() override;
  void SetAcceptTypes(std::vector<std::u16string> types) override;
  void SetUseMediaCapture(bool use_media_capture) override;
  void SetOpenWritable(bool open_writable) override;

  // Called when it is time to display the file picker.
  void SelectFileImpl(SelectFileDialog::Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const SelectFileDialog::FileTypeInfo* file_types,
                      int file_type_index,
                      const std::string& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;

 protected:
  ~SelectFileDialogImpl() override;

 private:
  SelectFileDialogImpl(Listener* listener,
                       std::unique_ptr<SelectFilePolicy> policy);

  bool HasMultipleFileTypeChoicesImpl() override;

  std::vector<std::u16string> accept_types_;
  bool use_media_capture_ = false;
  bool open_writable_ = false;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_ANDROID_H_

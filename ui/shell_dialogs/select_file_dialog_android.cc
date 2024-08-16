// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "select_file_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/android/window_android.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/select_file_dialog_jni_headers/SelectFileDialog_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace ui {
namespace {

constexpr char kGetContent[] = "android.intent.action.GET_CONTENT";
constexpr char kOpenDocument[] = "android.intent.action.OPEN_DOCUMENT";
constexpr char kOpenDocumentTree[] = "android.intent.action.OPEN_DOCUMENT_TREE";

std::string IntentActionFromType(SelectFileDialog::Type type,
                                 bool open_writable) {
  switch (type) {
    case SelectFileDialog::SELECT_NONE:
      return kGetContent;
    case SelectFileDialog::SELECT_OPEN_FILE:
    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      return open_writable ? kOpenDocument : kGetContent;
    case SelectFileDialog::SELECT_SAVEAS_FILE:
      return kOpenDocument;
    case SelectFileDialog::SELECT_FOLDER:
    case SelectFileDialog::SELECT_UPLOAD_FOLDER:
    case SelectFileDialog::SELECT_EXISTING_FOLDER:
      return kOpenDocumentTree;
  }
}

}  // namespace

// static
SelectFileDialogImpl* SelectFileDialogImpl::Create(
    Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  return new SelectFileDialogImpl(listener, std::move(policy));
}

void SelectFileDialogImpl::OnFileSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    const JavaParamRef<jstring>& filepath,
    const JavaParamRef<jstring>& display_name) {
  if (!listener_)
    return;

  std::string path = ConvertJavaStringToUTF8(env, filepath);
  std::string file_name = ConvertJavaStringToUTF8(env, display_name);
  base::FilePath file_path = base::FilePath(path);
  ui::SelectedFileInfo file_info(file_path);
  if (!file_name.empty())
    file_info.display_name = file_name;

  listener_->FileSelected(file_info, 0);
}

void SelectFileDialogImpl::OnMultipleFilesSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    const JavaParamRef<jobjectArray>& filepaths,
    const JavaParamRef<jobjectArray>& display_names) {
  if (!listener_)
    return;

  std::vector<ui::SelectedFileInfo> selected_files;

  jsize length = env->GetArrayLength(filepaths);
  DCHECK(length == env->GetArrayLength(display_names));
  for (int i = 0; i < length; ++i) {
    ScopedJavaLocalRef<jstring> path_ref(
        env, static_cast<jstring>(env->GetObjectArrayElement(filepaths, i)));
    base::FilePath file_path =
        base::FilePath(ConvertJavaStringToUTF8(env, path_ref));

    ScopedJavaLocalRef<jstring> display_name_ref(
        env,
        static_cast<jstring>(env->GetObjectArrayElement(display_names, i)));
    std::string display_name =
        ConvertJavaStringToUTF8(env, display_name_ref.obj());

    ui::SelectedFileInfo file_info(file_path);
    file_info.display_name = display_name;

    selected_files.push_back(file_info);
  }

  listener_->MultiFilesSelected(selected_files);
}

void SelectFileDialogImpl::OnFileNotSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object) {
  if (listener_)
    listener_->FileSelectionCanceled();
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow) const {
  return listener_;
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = nullptr;
}

void SelectFileDialogImpl::SetAcceptTypes(std::vector<std::u16string> types) {
  accept_types_ = std::move(types);
}

void SelectFileDialogImpl::SetUseMediaCapture(bool use_media_capture) {
  use_media_capture_ = use_media_capture;
}

void SelectFileDialogImpl::SetOpenWritable(bool open_writable) {
  open_writable_ = open_writable;
}

void SelectFileDialogImpl::SelectFileImpl(
    SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const std::string& default_extension,
    gfx::NativeWindow owning_window,
    const GURL* caller) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> intent_action =
      base::android::ConvertUTF8ToJavaString(
          env, IntentActionFromType(type, open_writable_));
  ScopedJavaLocalRef<jobjectArray> accept_types_java =
      base::android::ToJavaArrayOfStrings(env, accept_types_);

  bool accept_multiple_files = SelectFileDialog::SELECT_OPEN_MULTI_FILE == type;

  Java_SelectFileDialog_selectFile(
      env, java_object_, intent_action, accept_types_java, use_media_capture_,
      accept_multiple_files, owning_window->GetJavaObject());
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SelectFileDialog_nativeDestroyed(env, java_object_);
}

SelectFileDialogImpl::SelectFileDialogImpl(
    Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(crbug.com/1365766): The `intptr_t` to `this` might get stale.
  java_object_.Reset(
      Java_SelectFileDialog_create(env, reinterpret_cast<intptr_t>(this)));
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  NOTIMPLEMENTED();
  return false;
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  return SelectFileDialogImpl::Create(listener, std::move(policy));
}

}  // namespace ui

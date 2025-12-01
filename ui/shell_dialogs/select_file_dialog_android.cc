// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "select_file_dialog_android.h"

#include "base/android/content_uri_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/notimplemented.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
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
constexpr char kCreateDocument[] = "android.intent.action.CREATE_DOCUMENT";
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
      return kCreateDocument;
    case SelectFileDialog::SELECT_FOLDER:
    case SelectFileDialog::SELECT_UPLOAD_FOLDER:
    case SelectFileDialog::SELECT_EXISTING_FOLDER:
      return kOpenDocumentTree;
  }
}

bool IsSelectingFolder(SelectFileDialog::Type type) {
  return type == SelectFileDialog::SELECT_FOLDER ||
         type == SelectFileDialog::SELECT_UPLOAD_FOLDER ||
         type == SelectFileDialog::SELECT_EXISTING_FOLDER;
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
    const JavaParamRef<jstring>& filepath,
    const JavaParamRef<jstring>& display_name) {
  if (!listener_)
    return;

  std::string path = ConvertJavaStringToUTF8(env, filepath);
  std::string file_name = ConvertJavaStringToUTF8(env, display_name);
  base::FilePath file_path = base::FilePath(path);

  if (IsSelectingFolder(select_type_)) {
    std::optional<base::FilePath> virtual_path =
        base::ResolveToVirtualDocumentPath(file_path);
    CHECK(virtual_path);
    file_path = *virtual_path;
  }

  ui::SelectedFileInfo file_info(file_path);
  if (!file_name.empty())
    file_info.display_name = file_name;

  listener_->FileSelected(file_info, 0);
}

void SelectFileDialogImpl::OnMultipleFilesSelected(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& filepaths,
    const JavaParamRef<jobjectArray>& display_names) {
  if (!listener_)
    return;

  std::vector<ui::SelectedFileInfo> selected_files;

  jsize length = env->GetArrayLength(filepaths.obj());
  DCHECK(length == env->GetArrayLength(display_names.obj()));
  for (int i = 0; i < length; ++i) {
    auto path_ref = ScopedJavaLocalRef<jstring>::Adopt(
        env,
        static_cast<jstring>(env->GetObjectArrayElement(filepaths.obj(), i)));
    base::FilePath file_path =
        base::FilePath(ConvertJavaStringToUTF8(env, path_ref));

    auto display_name_ref = ScopedJavaLocalRef<jstring>::Adopt(
        env, static_cast<jstring>(
                 env->GetObjectArrayElement(display_names.obj(), i)));
    std::string display_name =
        ConvertJavaStringToUTF8(env, display_name_ref.obj());

    if (IsSelectingFolder(select_type_)) {
      std::optional<base::FilePath> virtual_path =
          base::ResolveToVirtualDocumentPath(file_path);
      CHECK(virtual_path);
      file_path = *virtual_path;
    }

    ui::SelectedFileInfo file_info(file_path);
    file_info.display_name = display_name;

    selected_files.push_back(file_info);
  }

  listener_->MultiFilesSelected(selected_files);
}

void SelectFileDialogImpl::OnFileNotSelected(JNIEnv* env) {
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

void SelectFileDialogImpl::SelectFileImpl(Type type,
                                          const std::u16string& title,
                                          const base::FilePath& default_path,
                                          const FileTypeInfo* file_types,
                                          int file_type_index,
                                          const std::string& default_extension,
                                          gfx::NativeWindow owning_window,
                                          const GURL* caller) {
  JNIEnv* env = base::android::AttachCurrentThread();

  select_type_ = type;
  ScopedJavaLocalRef<jstring> intent_action_java =
      base::android::ConvertUTF8ToJavaString(
          env, IntentActionFromType(select_type_, open_writable_));
  ScopedJavaLocalRef<jobjectArray> accept_types_java =
      base::android::ToJavaArrayOfStrings(env, accept_types_);

  bool accept_multiple_files = SelectFileDialog::SELECT_OPEN_MULTI_FILE == type;

  base::FilePath default_directory;
  base::FilePath suggested_name;
  // If default_path ends with a separator, then suggested_name was empty.
  if (default_path.EndsWithSeparator()) {
    default_directory = default_path;
  } else {
    default_directory = default_path.DirName();
    suggested_name = default_path.BaseName();
  }
  if (!default_directory.IsContentUri()) {
    default_directory = base::FilePath();
  }

  Java_SelectFileDialog_selectFile(
      env, java_object_, intent_action_java, accept_types_java,
      use_media_capture_, accept_multiple_files, default_directory.value(),
      suggested_name.value(), owning_window->GetJavaObject());
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

DEFINE_JNI(SelectFileDialog)

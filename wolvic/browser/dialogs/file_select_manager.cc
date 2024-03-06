// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/dialogs/file_select_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "wolvic/jni_headers/FileSelectManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ScopedJavaLocalRef;

namespace wolvic {

// static
void FileSelectManager::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // FileSelectManager will keep itself alive until it sends the result
  // message.
  scoped_refptr<FileSelectManager> file_select_manager(new FileSelectManager());
  file_select_manager->RunSelectFile(render_frame_host, std::move(listener),
                                     params);
}

FileSelectManager::FileSelectManager() = default;

FileSelectManager::~FileSelectManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (java_impl_) {
    JNIEnv* env = AttachCurrentThread();
    Java_FileSelectManager_destroyed(env, java_impl_);
  }
}

void FileSelectManager::OnFileSelected(
    JNIEnv* env, const JavaParamRef<jstring>& filepath) {
  std::string path = ConvertJavaStringToUTF8(env, filepath);
  ConvertToFileChooserFileInfoList({base::FilePath(path)});
}

void FileSelectManager::OnMultipleFilesSelected(
    JNIEnv* env, const JavaParamRef<jobjectArray>& filepaths) {
  std::vector<base::FilePath> selected_files;
  jsize length = env->GetArrayLength(filepaths);
  for (int i = 0; i < length; ++i) {
    ScopedJavaLocalRef<jstring> path_ref(
        env, static_cast<jstring>(env->GetObjectArrayElement(filepaths, i)));
    base::FilePath file_path =
        base::FilePath(ConvertJavaStringToUTF8(env, path_ref));
    selected_files.push_back(file_path);
  }
  ConvertToFileChooserFileInfoList(selected_files);
}

void FileSelectManager::OnFileSelectionCanceled(JNIEnv* env) {
  RunSelectFileEnd();
}

void FileSelectManager::RunSelectFile(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  DCHECK(!web_contents_);
  DCHECK(listener);
  DCHECK(!listener_);
  DCHECK(!java_impl_);

  listener_ = std::move(listener);
  web_contents_ = content::WebContents::FromRenderFrameHost(render_frame_host)
                      ->GetWeakPtr();

  dialog_mode_ = params.mode;

  std::pair<std::vector<std::u16string>, bool> accept_types =
      std::make_pair(params.accept_types, params.use_media_capture);

  JNIEnv* env = AttachCurrentThread();
  java_impl_ = Java_FileSelectManager_create(env, reinterpret_cast<intptr_t>(this));

  ScopedJavaLocalRef<jobjectArray> accept_types_java =
      ToJavaArrayOfStrings(env, params.accept_types);
  bool accept_multiple_files =
      blink::mojom::FileChooserParams::Mode::kOpenMultiple == params.mode;
  Java_FileSelectManager_selectFile(env, java_impl_, accept_types_java,
                                    params.use_media_capture,
                                    accept_multiple_files);

  // Because this class returns notifications to the RenderViewHost, it is
  // difficult for callers to know how long to keep a reference to this
  // instance. We AddRef() here to keep the instance alive after we return
  // to the caller, until the last callback is received from the file dialog
  // on Android.
  AddRef();
}

void FileSelectManager::RunSelectFileEnd() {
  if (listener_)
    listener_->FileSelectionCanceled();

  JNIEnv* env = AttachCurrentThread();
  Java_FileSelectManager_destroyed(env, java_impl_);
  java_impl_ = nullptr;
  Release();
}

void FileSelectManager::ConvertToFileChooserFileInfoList(
    const std::vector<base::FilePath>& files) {
  if (!web_contents_) {
    RunSelectFileEnd();
    return;
  }

  std::vector<blink::mojom::FileChooserFileInfoPtr> selected_files;
  for (const auto& file : files) {
    selected_files.push_back(
        blink::mojom::FileChooserFileInfo::NewNativeFile(
            blink::mojom::NativeFileInfo::New(file, file.BaseName().AsUTF16Unsafe())));
  }

  listener_->FileSelected(std::move(selected_files), base::FilePath(),
                          dialog_mode_);
  listener_ = nullptr;

  // No members should be accessed from here on.
  RunSelectFileEnd();
}

}  // namespace wolvic

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "ui/gfx/android/java_bitmap.h"
#include "weblayer/browser/java/jni/DownloadImpl_jni.h"
#endif

namespace weblayer {

DownloadImpl::~DownloadImpl() {
#if BUILDFLAG(IS_ANDROID)
  if (java_download_) {
    Java_DownloadImpl_onNativeDestroyed(base::android::AttachCurrentThread(),
                                        java_download_);
  }
#endif
}

#if BUILDFLAG(IS_ANDROID)
void DownloadImpl::SetJavaDownload(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_download) {
  java_download_.Reset(env, java_download);
}

base::android::ScopedJavaLocalRef<jstring> DownloadImpl::GetLocationImpl(
    JNIEnv* env) {
  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetLocation().value()));
}

base::android::ScopedJavaLocalRef<jstring>
DownloadImpl::GetFileNameToReportToUserImpl(JNIEnv* env) {
  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF16ToJavaString(env,
                                              GetFileNameToReportToUser()));
}

base::android::ScopedJavaLocalRef<jstring> DownloadImpl::GetMimeTypeImpl(
    JNIEnv* env) {
  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetMimeType()));
}

base::android::ScopedJavaLocalRef<jobject> DownloadImpl::GetLargeIconImpl(
    JNIEnv* env) {
  base::android::ScopedJavaLocalRef<jobject> j_icon;
  const SkBitmap* icon = GetLargeIcon();

  if (icon && !icon->drawsNothing())
    j_icon = gfx::ConvertToJavaBitmap(*icon);

  return j_icon;
}
#endif

DownloadImpl::DownloadImpl() = default;

bool DownloadImpl::HasBeenAddedToUi() {
#if BUILDFLAG(IS_ANDROID)
  return static_cast<bool>(java_download_);
#else
  // Since there is no UI outside of Android, we'll assume true.
  return true;
#endif
}

}  // namespace weblayer

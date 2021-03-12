// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DOWNLOAD_IMPL_H_
#define WEBLAYER_BROWSER_DOWNLOAD_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "weblayer/public/download.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace weblayer {

// Base class for downloads that should be represented in the UI.
class DownloadImpl : public Download, public base::SupportsUserData::Data {
 public:
  ~DownloadImpl() override;
  DownloadImpl(const DownloadImpl&) = delete;
  DownloadImpl& operator=(const DownloadImpl&) = delete;

#if defined(OS_ANDROID)
  void SetJavaDownload(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_download);
  int GetStateImpl(JNIEnv* env) { return static_cast<int>(GetState()); }
  jlong GetTotalBytesImpl(JNIEnv* env) { return GetTotalBytes(); }
  jlong GetReceivedBytesImpl(JNIEnv* env) { return GetReceivedBytes(); }
  void PauseImpl(JNIEnv* env) { Pause(); }
  void ResumeImpl(JNIEnv* env) { Resume(); }
  void CancelImpl(JNIEnv* env) { Cancel(); }
  base::android::ScopedJavaLocalRef<jstring> GetLocationImpl(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetFileNameToReportToUserImpl(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetMimeTypeImpl(JNIEnv* env);
  int GetErrorImpl(JNIEnv* env) { return static_cast<int>(GetError()); }
  bool IsTransientImpl(JNIEnv* env) { return IsTransient(); }

  base::android::ScopedJavaGlobalRef<jobject> java_download() {
    return java_download_;
  }
#endif

  // Returns an ID suitable for use as an Android notification ID. This must be
  // unique across all DownloadImpls.
  virtual int GetNotificationId() = 0;

  // A transient download is not persisted to disk, which will affect its UI
  // treatment.
  virtual bool IsTransient() = 0;

  // Returns whether this download has been added to the UI via
  // DownloadDelegate::OnDownloadStarted.
  bool HasBeenAddedToUi();

 protected:
  DownloadImpl();

 private:
#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_download_;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DOWNLOAD_IMPL_H_

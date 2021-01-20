// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DOWNLOAD_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_DOWNLOAD_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "weblayer/public/download_delegate.h"

namespace weblayer {

class Profile;

// Forwards DownloadDelegate calls to the java-side DownloadCallbackProxy.
class DownloadCallbackProxy : public DownloadDelegate {
 public:
  DownloadCallbackProxy(JNIEnv* env, jobject obj, Profile* profile);
  ~DownloadCallbackProxy() override;

  // DownloadDelegate:
  bool InterceptDownload(const GURL& url,
                         const std::string& user_agent,
                         const std::string& content_disposition,
                         const std::string& mime_type,
                         int64_t content_length) override;
  void AllowDownload(Tab* tab,
                     const GURL& url,
                     const std::string& request_method,
                     base::Optional<url::Origin> request_initiator,
                     AllowDownloadCallback callback) override;
  void DownloadStarted(Download* download) override;
  void DownloadProgressChanged(Download* download) override;
  void DownloadCompleted(Download* download) override;
  void DownloadFailed(Download* download) override;

 private:
  Profile* profile_;
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DownloadCallbackProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DOWNLOAD_CALLBACK_PROXY_H_

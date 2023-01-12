// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DOWNLOAD_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_DOWNLOAD_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "weblayer/public/download_delegate.h"

namespace weblayer {

class Profile;

// Forwards DownloadDelegate calls to the java-side DownloadCallbackProxy.
class DownloadCallbackProxy : public DownloadDelegate {
 public:
  DownloadCallbackProxy(JNIEnv* env, jobject obj, Profile* profile);

  DownloadCallbackProxy(const DownloadCallbackProxy&) = delete;
  DownloadCallbackProxy& operator=(const DownloadCallbackProxy&) = delete;

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
                     absl::optional<url::Origin> request_initiator,
                     AllowDownloadCallback callback) override;
  void DownloadStarted(Download* download) override;
  void DownloadProgressChanged(Download* download) override;
  void DownloadCompleted(Download* download) override;
  void DownloadFailed(Download* download) override;

 private:
  raw_ptr<Profile> profile_;
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DOWNLOAD_CALLBACK_PROXY_H_

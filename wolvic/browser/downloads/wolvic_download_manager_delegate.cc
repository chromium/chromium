// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/downloads/wolvic_download_manager_delegate.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "wolvic/jni_headers/DownloadManagerBridge_jni.h"

namespace wolvic {

using base::android::ScopedJavaLocalRef;

bool WolvicDownloadManagerDelegate::InterceptDownloadIfApplicable(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& request_origin,
    int64_t content_length,
    bool is_transient,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!web_contents) {
    return true;
  }

  DVLOG(1) << "Forwarding the download to Wolvic: " << url.spec();

  ScopedJavaLocalRef<jstring> jstring_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  Java_DownloadManagerBridge_newDownload(env, jstring_url);
  return true;
}

}  // namespace wolvic

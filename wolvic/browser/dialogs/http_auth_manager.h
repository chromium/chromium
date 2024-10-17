// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_DIALOGS_HTTP_AUTH_MANAGER_H_
#define WOLVIC_BROWSER_DIALOGS_HTTP_AUTH_MANAGER_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/login_delegate.h"

namespace wolvic {
class WebContents;
}

namespace net {
class AuthChallengeInfo;
}

namespace wolvic {

// Implements support for http auth.
class HttpAuthManager : public content::LoginDelegate {
 public:
  HttpAuthManager(const net::AuthChallengeInfo& auth_info,
                      content::WebContents* web_contents,
                      bool first_auth_attempt,
                      LoginAuthRequiredCallback callback);
  ~HttpAuthManager() override;

  void Proceed(JNIEnv* env,
               const base::android::JavaParamRef<jstring>& username,
               const base::android::JavaParamRef<jstring>& password);
  void Cancel(JNIEnv* env);

 private:
  void CloseDialog();

  LoginAuthRequiredCallback callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_DIALOGS_HTTP_AUTH_MANAGER_H_

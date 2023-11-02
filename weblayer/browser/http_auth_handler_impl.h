// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_HTTP_AUTH_HANDLER_IMPL_H_
#define WEBLAYER_BROWSER_HTTP_AUTH_HANDLER_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/login_delegate.h"
#include "url/gurl.h"

namespace weblayer {

// Implements support for http auth.
class HttpAuthHandlerImpl : public content::LoginDelegate {
 public:
  HttpAuthHandlerImpl(const net::AuthChallengeInfo& auth_info,
                      content::WebContents* web_contents,
                      bool first_auth_attempt,
                      LoginAuthRequiredCallback callback);
  ~HttpAuthHandlerImpl() override;

  void Proceed(JNIEnv* env,
               const base::android::JavaParamRef<jstring>& username,
               const base::android::JavaParamRef<jstring>& password);
  void Cancel(JNIEnv* env);

 private:
  void CloseDialog();

  GURL url_;
  LoginAuthRequiredCallback callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_HTTP_AUTH_HANDLER_IMPL_H_

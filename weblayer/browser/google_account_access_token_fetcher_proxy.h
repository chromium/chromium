// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_GOOGLE_ACCOUNT_ACCESS_TOKEN_FETCHER_PROXY_H_
#define WEBLAYER_BROWSER_GOOGLE_ACCOUNT_ACCESS_TOKEN_FETCHER_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "weblayer/public/google_account_access_token_fetch_delegate.h"

namespace weblayer {

class Profile;

// Forwards GoogleAccountAccessTokenFetchDelegate calls to the java-side
// GoogleAccountAccessTokenFetcherProxy.
class GoogleAccountAccessTokenFetcherProxy
    : public GoogleAccountAccessTokenFetchDelegate {
 public:
  GoogleAccountAccessTokenFetcherProxy(JNIEnv* env,
                                       jobject obj,
                                       Profile* profile);
  ~GoogleAccountAccessTokenFetcherProxy() override;

  GoogleAccountAccessTokenFetcherProxy(
      const GoogleAccountAccessTokenFetcherProxy&) = delete;
  GoogleAccountAccessTokenFetcherProxy& operator=(
      const GoogleAccountAccessTokenFetcherProxy&) = delete;

  // GoogleAccountAccessTokenFetchDelegate:
  void FetchAccessToken(const std::set<std::string>& scopes,
                        OnTokenFetchedCallback callback) override;
  void OnAccessTokenIdentifiedAsInvalid(const std::set<std::string>& scopes,
                                        const std::string& token) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  raw_ptr<Profile> profile_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_GOOGLE_ACCOUNT_ACCESS_TOKEN_FETCHER_PROXY_H_

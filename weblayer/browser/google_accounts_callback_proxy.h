// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_GOOGLE_ACCOUNTS_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_GOOGLE_ACCOUNTS_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "weblayer/public/google_accounts_delegate.h"

namespace weblayer {

class Tab;

class GoogleAccountsCallbackProxy : public GoogleAccountsDelegate {
 public:
  GoogleAccountsCallbackProxy(JNIEnv* env, jobject obj, Tab* tab);
  ~GoogleAccountsCallbackProxy() override;

  // GoogleAccountsDelegate:
  void OnGoogleAccountsRequest(
      const signin::ManageAccountsParams& params) override;
  std::string GetGaiaId() override;

 private:
  raw_ptr<Tab> tab_;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_GOOGLE_ACCOUNTS_CALLBACK_PROXY_H_

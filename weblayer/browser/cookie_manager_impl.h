// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_COOKIE_MANAGER_IMPL_H_
#define WEBLAYER_BROWSER_COOKIE_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "weblayer/public/cookie_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class BrowserContext;
}

namespace weblayer {

class CookieManagerImpl : public CookieManager {
 public:
  explicit CookieManagerImpl(content::BrowserContext* browser_context);
  ~CookieManagerImpl() override;

  CookieManagerImpl(const CookieManagerImpl&) = delete;
  CookieManagerImpl& operator=(const CookieManagerImpl&) = delete;

  // CookieManager implementation:
  void SetCookie(const GURL& url,
                 const std::string& value,
                 SetCookieCallback callback) override;
  void GetCookie(const GURL& url, GetCookieCallback callback) override;
  void GetResponseCookies(const GURL& url,
                          GetResponseCookiesCallback callback) override;
  base::CallbackListSubscription AddCookieChangedCallback(
      const GURL& url,
      const std::string* name,
      CookieChangedCallback callback) override;

#if BUILDFLAG(IS_ANDROID)
  void SetCookie(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& url,
                 const base::android::JavaParamRef<jstring>& value,
                 const base::android::JavaParamRef<jobject>& callback);
  void GetCookie(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& url,
                 const base::android::JavaParamRef<jobject>& callback);
  void GetResponseCookies(JNIEnv* env,
                          const base::android::JavaParamRef<jstring>& url,
                          const base::android::JavaParamRef<jobject>& callback);
  int AddCookieChangedCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& name,
      const base::android::JavaParamRef<jobject>& callback);
  void RemoveCookieChangedCallback(JNIEnv* env, int id);
#endif

  // Fires the cookie flush timer immediately and waits for the flush to
  // complete. Returns true if the flush timer was running.
  bool FireFlushTimerForTesting();

 private:
  void SetCookieInternal(const GURL& url,
                         const std::string& value,
                         SetCookieCallback callback);
  int AddCookieChangedCallbackInternal(const GURL& url,
                                       const std::string* name,
                                       CookieChangedCallback callback);
  void RemoveCookieChangedCallbackInternal(int id);
  void OnCookieSet(SetCookieCallback callback, bool success);
  void OnFlushTimerFired();

  raw_ptr<content::BrowserContext> browser_context_;
  mojo::ReceiverSet<network::mojom::CookieChangeListener,
                    std::unique_ptr<network::mojom::CookieChangeListener>>
      cookie_change_receivers_;

  std::unique_ptr<base::OneShotTimer> flush_timer_;
  std::unique_ptr<base::RunLoop> flush_run_loop_for_testing_;

  base::WeakPtrFactory<CookieManagerImpl> weak_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_COOKIE_MANAGER_IMPL_H_

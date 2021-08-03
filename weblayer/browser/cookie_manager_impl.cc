// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/cookie_manager_impl.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_util.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "weblayer/browser/java/jni/CookieManagerImpl_jni.h"
#endif

namespace weblayer {
namespace {
constexpr base::TimeDelta kCookieFlushDelay = base::TimeDelta::FromSeconds(1);

void GetCookieComplete(CookieManager::GetCookieCallback callback,
                       const net::CookieAccessResultList& cookies,
                       const net::CookieAccessResultList& excluded_cookies) {
  net::CookieList cookie_list = net::cookie_util::StripAccessResults(cookies);
  std::move(callback).Run(net::CanonicalCookie::BuildCookieLine(cookie_list));
}

#if defined(OS_ANDROID)
void OnCookieChangedAndroid(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const net::CookieChangeInfo& change) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CookieManagerImpl_onCookieChange(
      env, callback,
      base::android::ConvertUTF8ToJavaString(
          env, net::CanonicalCookie::BuildCookieLine({change.cookie})),
      static_cast<int>(change.cause));
}
#endif

void OnCookieChanged(CookieManager::CookieChangedCallbackList* callback_list,
                     const net::CookieChangeInfo& change) {
  callback_list->Notify(change);
}

class CookieChangeListenerImpl : public network::mojom::CookieChangeListener {
 public:
  explicit CookieChangeListenerImpl(
      CookieManager::CookieChangedCallback callback)
      : callback_(std::move(callback)) {}

  // mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    callback_.Run(change);
  }

 private:
  CookieManager::CookieChangedCallback callback_;
};

}  // namespace

CookieManagerImpl::CookieManagerImpl(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

CookieManagerImpl::~CookieManagerImpl() = default;

void CookieManagerImpl::SetCookie(const GURL& url,
                                  const std::string& value,
                                  SetCookieCallback callback) {
  CHECK(SetCookieInternal(url, value, std::move(callback)));
}

void CookieManagerImpl::GetCookie(const GURL& url, GetCookieCallback callback) {
  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(url, net::CookieOptions::MakeAllInclusive(),
                      base::BindOnce(&GetCookieComplete, std::move(callback)));
}

base::CallbackListSubscription CookieManagerImpl::AddCookieChangedCallback(
    const GURL& url,
    const std::string* name,
    CookieChangedCallback callback) {
  auto callback_list = std::make_unique<CookieChangedCallbackList>();
  auto* callback_list_ptr = callback_list.get();
  int id = AddCookieChangedCallbackInternal(
      url, name,
      base::BindRepeating(&OnCookieChanged,
                          base::Owned(std::move(callback_list))));
  callback_list_ptr->set_removal_callback(base::BindRepeating(
      &CookieManagerImpl::RemoveCookieChangedCallbackInternal,
      weak_factory_.GetWeakPtr(), id));
  return callback_list_ptr->Add(std::move(callback));
}

#if defined(OS_ANDROID)
bool CookieManagerImpl::SetCookie(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jstring>& value,
    const base::android::JavaParamRef<jobject>& callback) {
  return SetCookieInternal(
      GURL(ConvertJavaStringToUTF8(url)), ConvertJavaStringToUTF8(value),
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

void CookieManagerImpl::GetCookie(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jobject>& callback) {
  GetCookie(
      GURL(ConvertJavaStringToUTF8(url)),
      base::BindOnce(&base::android::RunStringCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

int CookieManagerImpl::AddCookieChangedCallback(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jobject>& callback) {
  std::string name_str;
  if (name)
    name_str = ConvertJavaStringToUTF8(name);
  return AddCookieChangedCallbackInternal(
      GURL(ConvertJavaStringToUTF8(url)), name ? &name_str : nullptr,
      base::BindRepeating(
          &OnCookieChangedAndroid,
          base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

void CookieManagerImpl::RemoveCookieChangedCallback(JNIEnv* env, int id) {
  RemoveCookieChangedCallbackInternal(id);
}
#endif

bool CookieManagerImpl::FireFlushTimerForTesting() {
  if (!flush_timer_)
    return false;

  flush_run_loop_for_testing_ = std::make_unique<base::RunLoop>();
  flush_timer_->FireNow();
  flush_run_loop_for_testing_->Run();
  flush_run_loop_for_testing_ = nullptr;
  return true;
}

bool CookieManagerImpl::SetCookieInternal(const GURL& url,
                                          const std::string& value,
                                          SetCookieCallback callback) {
  auto cc =
      net::CanonicalCookie::Create(url, value, base::Time::Now(), absl::nullopt,
                                   net::CookiePartitionKey::Todo());
  if (!cc) {
    return false;
  }

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetCanonicalCookie(
          *cc, url, net::CookieOptions::MakeAllInclusive(),
          net::cookie_util::AdaptCookieAccessResultToBool(
              base::BindOnce(&CookieManagerImpl::OnCookieSet,
                             weak_factory_.GetWeakPtr(), std::move(callback))));
  return true;
}

int CookieManagerImpl::AddCookieChangedCallbackInternal(
    const GURL& url,
    const std::string* name,
    CookieChangedCallback callback) {
  mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
  auto receiver = listener_remote.InitWithNewPipeAndPassReceiver();
  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->AddCookieChangeListener(
          url, name ? absl::make_optional(*name) : absl::nullopt,
          std::move(listener_remote));

  auto listener =
      std::make_unique<CookieChangeListenerImpl>(std::move(callback));
  auto* listener_ptr = listener.get();
  return cookie_change_receivers_.Add(listener_ptr, std::move(receiver),
                                      std::move(listener));
}

void CookieManagerImpl::RemoveCookieChangedCallbackInternal(int id) {
  cookie_change_receivers_.Remove(id);
}

void CookieManagerImpl::OnCookieSet(SetCookieCallback callback, bool success) {
  std::move(callback).Run(success);
  if (!flush_timer_) {
    flush_timer_ = std::make_unique<base::OneShotTimer>();
    flush_timer_->Start(FROM_HERE, kCookieFlushDelay,
                        base::BindOnce(&CookieManagerImpl::OnFlushTimerFired,
                                       weak_factory_.GetWeakPtr()));
  }
}

void CookieManagerImpl::OnFlushTimerFired() {
  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->FlushCookieStore(flush_run_loop_for_testing_
                             ? flush_run_loop_for_testing_->QuitClosure()
                             : base::DoNothing());
  flush_timer_ = nullptr;
}

}  // namespace weblayer

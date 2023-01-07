// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_callback_proxy.h"

#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "weblayer/browser/download_impl.h"
#include "weblayer/browser/java/jni/DownloadCallbackProxy_jni.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

DownloadCallbackProxy::DownloadCallbackProxy(JNIEnv* env,
                                             jobject obj,
                                             Profile* profile)
    : profile_(profile), java_delegate_(env, obj) {
  profile_->SetDownloadDelegate(this);
}

DownloadCallbackProxy::~DownloadCallbackProxy() {
  profile_->SetDownloadDelegate(nullptr);
}

bool DownloadCallbackProxy::InterceptDownload(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    int64_t content_length) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));
  ScopedJavaLocalRef<jstring> jstring_user_agent(
      ConvertUTF8ToJavaString(env, user_agent));
  ScopedJavaLocalRef<jstring> jstring_content_disposition(
      ConvertUTF8ToJavaString(env, content_disposition));
  ScopedJavaLocalRef<jstring> jstring_mime_type(
      ConvertUTF8ToJavaString(env, mime_type));
  TRACE_EVENT0("weblayer", "Java_DownloadCallbackProxy_interceptDownload");
  return Java_DownloadCallbackProxy_interceptDownload(
      env, java_delegate_, jstring_url, jstring_user_agent,
      jstring_content_disposition, jstring_mime_type, content_length);
}

void DownloadCallbackProxy::AllowDownload(
    Tab* tab,
    const GURL& url,
    const std::string& request_method,
    absl::optional<url::Origin> request_initiator,
    AllowDownloadCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));
  ScopedJavaLocalRef<jstring> jstring_method(
      ConvertUTF8ToJavaString(env, request_method));
  ScopedJavaLocalRef<jstring> jstring_request_initator;
  if (request_initiator)
    jstring_request_initator =
        ConvertUTF8ToJavaString(env, request_initiator->Serialize());
  // Make copy on the heap so we can pass the pointer through JNI. This will be
  // deleted when it's run.
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new AllowDownloadCallback(std::move(callback)));
  Java_DownloadCallbackProxy_allowDownload(
      env, java_delegate_, static_cast<TabImpl*>(tab)->GetJavaTab(),
      jstring_url, jstring_method, jstring_request_initator, callback_id);
}

void DownloadCallbackProxy::DownloadStarted(Download* download) {
  DownloadImpl* download_impl = static_cast<DownloadImpl*>(download);
  JNIEnv* env = AttachCurrentThread();
  Java_DownloadCallbackProxy_createDownload(
      env, java_delegate_, reinterpret_cast<jlong>(download_impl),
      download_impl->GetNotificationId(), download_impl->IsTransient(),
      url::GURLAndroid::FromNativeGURL(env, download_impl->GetSourceUrl()));
  Java_DownloadCallbackProxy_downloadStarted(env, java_delegate_,
                                             download_impl->java_download());
}

void DownloadCallbackProxy::DownloadProgressChanged(Download* download) {
  DownloadImpl* download_impl = static_cast<DownloadImpl*>(download);
  Java_DownloadCallbackProxy_downloadProgressChanged(
      AttachCurrentThread(), java_delegate_, download_impl->java_download());
}

void DownloadCallbackProxy::DownloadCompleted(Download* download) {
  DownloadImpl* download_impl = static_cast<DownloadImpl*>(download);
  Java_DownloadCallbackProxy_downloadCompleted(
      AttachCurrentThread(), java_delegate_, download_impl->java_download());
}

void DownloadCallbackProxy::DownloadFailed(Download* download) {
  DownloadImpl* download_impl = static_cast<DownloadImpl*>(download);
  Java_DownloadCallbackProxy_downloadFailed(
      AttachCurrentThread(), java_delegate_, download_impl->java_download());
}

static jlong JNI_DownloadCallbackProxy_CreateDownloadCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong profile) {
  return reinterpret_cast<jlong>(new DownloadCallbackProxy(
      env, proxy, reinterpret_cast<ProfileImpl*>(profile)));
}

static void JNI_DownloadCallbackProxy_DeleteDownloadCallbackProxy(JNIEnv* env,
                                                                  jlong proxy) {
  delete reinterpret_cast<DownloadCallbackProxy*>(proxy);
}

static void JNI_DownloadCallbackProxy_AllowDownload(JNIEnv* env,
                                                    jlong callback_id,
                                                    jboolean allow) {
  std::unique_ptr<AllowDownloadCallback> cb(
      reinterpret_cast<AllowDownloadCallback*>(callback_id));
  std::move(*cb).Run(allow);
}

}  // namespace weblayer

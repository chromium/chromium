// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/tab_callback_proxy.h"

#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "url/gurl.h"
#include "weblayer/browser/java/jni/TabCallbackProxy_jni.h"
#include "weblayer/browser/tab_impl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

TabCallbackProxy::TabCallbackProxy(JNIEnv* env, jobject obj, Tab* tab)
    : tab_(tab), java_observer_(env, obj) {
  tab_->AddObserver(this);
}

TabCallbackProxy::~TabCallbackProxy() {
  tab_->RemoveObserver(this);
}

void TabCallbackProxy::DisplayedUrlChanged(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_uri_string(
      ConvertUTF8ToJavaString(env, url.spec()));
  TRACE_EVENT0("weblayer", "Java_TabCallbackProxy_visibleUriChanged");
  Java_TabCallbackProxy_visibleUriChanged(env, java_observer_,
                                          jstring_uri_string);
}

void TabCallbackProxy::OnRenderProcessGone() {
  TRACE_EVENT0("weblayer", "Java_TabCallbackProxy_onRenderProcessGone");
  Java_TabCallbackProxy_onRenderProcessGone(AttachCurrentThread(),
                                            java_observer_);
}

void TabCallbackProxy::OnTitleUpdated(const std::u16string& title) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabCallbackProxy_onTitleUpdated(
      env, java_observer_, base::android::ConvertUTF16ToJavaString(env, title));
}

static jlong JNI_TabCallbackProxy_CreateTabCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong tab) {
  return reinterpret_cast<jlong>(
      new TabCallbackProxy(env, proxy, reinterpret_cast<TabImpl*>(tab)));
}

static void JNI_TabCallbackProxy_DeleteTabCallbackProxy(JNIEnv* env,
                                                        jlong proxy) {
  delete reinterpret_cast<TabCallbackProxy*>(proxy);
}

}  // namespace weblayer

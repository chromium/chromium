// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/new_tab_callback_proxy.h"

#include "url/gurl.h"
#include "weblayer/browser/java/jni/NewTabCallbackProxy_jni.h"
#include "weblayer/browser/tab_impl.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

NewTabCallbackProxy::NewTabCallbackProxy(JNIEnv* env, jobject obj, TabImpl* tab)
    : tab_(tab), java_impl_(env, obj) {
  DCHECK(!tab_->has_new_tab_delegate());
  tab_->SetNewTabDelegate(this);
}

NewTabCallbackProxy::~NewTabCallbackProxy() {
  tab_->SetNewTabDelegate(nullptr);
}

void NewTabCallbackProxy::OnNewTab(std::unique_ptr<Tab> tab, NewTabType type) {
  JNIEnv* env = AttachCurrentThread();
  // The Java side takes ownership of Tab.
  TRACE_EVENT0("weblayer", "Java_NewTabCallbackProxy_onNewTab");
  Java_NewTabCallbackProxy_onNewTab(env, java_impl_,
                                    reinterpret_cast<jlong>(tab.release()),
                                    static_cast<int>(type));
}

void NewTabCallbackProxy::CloseTab() {
  Java_NewTabCallbackProxy_onCloseTab(AttachCurrentThread(), java_impl_);
}

static jlong JNI_NewTabCallbackProxy_CreateNewTabCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong tab) {
  return reinterpret_cast<jlong>(
      new NewTabCallbackProxy(env, proxy, reinterpret_cast<TabImpl*>(tab)));
}

static void JNI_NewTabCallbackProxy_DeleteNewTabCallbackProxy(JNIEnv* env,
                                                              jlong proxy) {
  delete reinterpret_cast<NewTabCallbackProxy*>(proxy);
}

}  // namespace weblayer

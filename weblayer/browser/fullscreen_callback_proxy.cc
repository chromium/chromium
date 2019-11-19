// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/fullscreen_callback_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/java/jni/FullscreenCallbackProxy_jni.h"
#include "weblayer/browser/tab_impl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

FullscreenCallbackProxy::FullscreenCallbackProxy(JNIEnv* env,
                                                 jobject obj,
                                                 Tab* tab)
    : tab_(tab), java_delegate_(env, obj) {
  tab_->SetFullscreenDelegate(this);
}

FullscreenCallbackProxy::~FullscreenCallbackProxy() {
  tab_->SetFullscreenDelegate(nullptr);
}

void FullscreenCallbackProxy::EnterFullscreen(base::OnceClosure exit_closure) {
  exit_fullscreen_closure_ = std::move(exit_closure);
  TRACE_EVENT0("weblayer", "Java_FullscreenCallbackProxy_enterFullscreen");
  Java_FullscreenCallbackProxy_enterFullscreen(AttachCurrentThread(),
                                               java_delegate_);
}

void FullscreenCallbackProxy::ExitFullscreen() {
  TRACE_EVENT0("weblayer", "Java_FullscreenCallbackProxy_exitFullscreen");
  Java_FullscreenCallbackProxy_exitFullscreen(AttachCurrentThread(),
                                              java_delegate_);
}

void FullscreenCallbackProxy::DoExitFullscreen(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  if (exit_fullscreen_closure_)
    std::move(exit_fullscreen_closure_).Run();
}

static jlong JNI_FullscreenCallbackProxy_CreateFullscreenCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong tab) {
  return reinterpret_cast<jlong>(
      new FullscreenCallbackProxy(env, proxy, reinterpret_cast<TabImpl*>(tab)));
}

static void JNI_FullscreenCallbackProxy_DeleteFullscreenCallbackProxy(
    JNIEnv* env,
    jlong proxy) {
  delete reinterpret_cast<FullscreenCallbackProxy*>(proxy);
}

}  // namespace weblayer

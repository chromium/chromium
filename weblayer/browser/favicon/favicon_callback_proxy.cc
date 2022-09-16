// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_callback_proxy.h"

#include "base/android/jni_string.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"
#include "weblayer/browser/java/jni/FaviconCallbackProxy_jni.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/favicon_fetcher.h"

namespace weblayer {

FaviconCallbackProxy::FaviconCallbackProxy(JNIEnv* env, jobject obj, Tab* tab)
    : java_proxy_(env, obj),
      favicon_fetcher_(tab->CreateFaviconFetcher(this)) {}

FaviconCallbackProxy::~FaviconCallbackProxy() = default;

void FaviconCallbackProxy::OnFaviconChanged(const gfx::Image& image) {
  SkBitmap favicon = image.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FaviconCallbackProxy_onFaviconChanged(
      env, java_proxy_,
      favicon.empty() ? nullptr : gfx::ConvertToJavaBitmap(favicon));
}

static jlong JNI_FaviconCallbackProxy_CreateFaviconCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong tab) {
  return reinterpret_cast<jlong>(
      new FaviconCallbackProxy(env, proxy, reinterpret_cast<TabImpl*>(tab)));
}

static void JNI_FaviconCallbackProxy_DeleteFaviconCallbackProxy(JNIEnv* env,
                                                                jlong proxy) {
  delete reinterpret_cast<FaviconCallbackProxy*>(proxy);
}

}  // namespace weblayer

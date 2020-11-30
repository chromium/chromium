// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/java/test_jni/TestWebLayerImpl_jni.h"

#include <utility>

#include "base/android/callback_android.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "weblayer/browser/tab_impl.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;

namespace weblayer {
namespace {

void CheckMetadata(
    std::unique_ptr<content::RenderFrameSubmissionObserver> observer,
    int top_height,
    int bottom_height,
    const ScopedJavaGlobalRef<jobject>& runnable) {
  const cc::RenderFrameMetadata& last_metadata =
      observer->LastRenderFrameMetadata();
  if (last_metadata.top_controls_height == top_height &&
      last_metadata.bottom_controls_height == bottom_height) {
    base::android::RunRunnableAndroid(runnable);
    return;
  }
  auto* const observer_ptr = observer.get();
  observer_ptr->NotifyOnNextMetadataChange(
      base::BindOnce(&CheckMetadata, std::move(observer), top_height,
                     bottom_height, runnable));
}

}  // namespace

static void JNI_TestWebLayerImpl_WaitForBrowserControlsMetadataState(
    JNIEnv* env,
    jlong tab_impl,
    jint top_height,
    jint bottom_height,
    const base::android::JavaParamRef<jobject>& runnable) {
  TabImpl* tab = reinterpret_cast<TabImpl*>(tab_impl);
  auto observer = std::make_unique<content::RenderFrameSubmissionObserver>(
      tab->web_contents());
  CheckMetadata(std::move(observer), top_height, bottom_height,
                ScopedJavaGlobalRef<jobject>(runnable));
}

static void JNI_TestWebLayerImpl_SetIgnoreMissingKeyForTranslateManager(
    JNIEnv* env,
    jboolean ignore) {
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(ignore);
}

}  // namespace weblayer

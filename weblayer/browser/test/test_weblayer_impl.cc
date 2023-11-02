// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/java/test_jni/TestWebLayerImpl_jni.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/background_service/features.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/profile_impl.h"
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

static void JNI_TestWebLayerImpl_ExpediteDownloadService(JNIEnv* env) {
  static base::NoDestructor<base::test::ScopedFeatureList> feature_list;
  feature_list->InitAndEnableFeatureWithParameters(
      download::kDownloadServiceFeature, {{"start_up_delay_ms", "0"}});
}

static void JNI_TestWebLayerImpl_GrantLocationPermission(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jurl) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  for (auto* profile : ProfileImpl::GetAllProfiles()) {
    HostContentSettingsMapFactory::GetForBrowserContext(
        profile->GetBrowserContext())
        ->SetContentSettingDefaultScope(
            url, url, ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  }
}

}  // namespace weblayer

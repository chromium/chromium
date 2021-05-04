// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/weblayer_webapps_client.h"

#include <string>

#include "base/logging.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/security_state/content/content_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "weblayer/browser/java/jni/WebappsHelper_jni.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/guid.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "ui/android/color_helpers.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"
#endif

namespace weblayer {

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// static
void WebLayerWebappsClient::Create() {
  static base::NoDestructor<WebLayerWebappsClient> instance;
  instance.get();
}

security_state::SecurityLevel
WebLayerWebappsClient::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
  auto state = security_state::GetVisibleSecurityState(web_contents);
  return security_state::GetSecurityLevel(
      *state,
      /* used_policy_installed_certificate */ false);
}

infobars::ContentInfoBarManager*
WebLayerWebappsClient::GetInfoBarManagerForWebContents(
    content::WebContents* web_contents) {
  return infobars::ContentInfoBarManager::FromWebContents(web_contents);
}

webapps::WebappInstallSource WebLayerWebappsClient::GetInstallSource(
    content::WebContents* web_contents,
    webapps::InstallTrigger trigger) {
  NOTIMPLEMENTED();
  return webapps::WebappInstallSource::COUNT;
}

webapps::AppBannerManager* WebLayerWebappsClient::GetAppBannerManager(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return nullptr;
}

#if defined(OS_ANDROID)
bool WebLayerWebappsClient::IsInstallationInProgress(
    content::WebContents* web_contents,
    const GURL& manifest_url) {
  NOTIMPLEMENTED();
  return false;
}

bool WebLayerWebappsClient::CanShowAppBanners(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return false;
}

void WebLayerWebappsClient::OnWebApkInstallInitiatedFromAppMenu(
    content::WebContents* web_contents) {}

void WebLayerWebappsClient::InstallWebApk(
    content::WebContents* web_contents,
    const webapps::AddToHomescreenParams& params) {
  NOTIMPLEMENTED();
}

void WebLayerWebappsClient::InstallShortcut(
    content::WebContents* web_contents,
    const webapps::AddToHomescreenParams& params) {
  const webapps::ShortcutInfo& info = *params.shortcut_info;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_id =
      ConvertUTF8ToJavaString(env, base::GenerateGUID());
  ScopedJavaLocalRef<jstring> java_url =
      ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      ConvertUTF16ToJavaString(env, info.user_title);
  ScopedJavaLocalRef<jstring> java_best_primary_icon_url =
      ConvertUTF8ToJavaString(env, info.best_primary_icon_url.spec());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!params.primary_icon.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(params.primary_icon);
  Java_WebappsHelper_addShortcut(env, java_id, java_url, java_user_title,
                                 java_bitmap, params.has_maskable_primary_icon,
                                 info.source, java_best_primary_icon_url);
}
#endif

}  // namespace weblayer

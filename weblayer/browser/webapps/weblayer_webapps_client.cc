// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/weblayer_webapps_client.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/security_state/content/content_utils.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_context.h"
#include "ui/android/color_utils_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "weblayer/browser/webapps/webapk_install_scheduler.h"
#include "weblayer/browser/webapps/webapps_utils.h"
#include "weblayer/browser/webapps/weblayer_app_banner_manager_android.h"

namespace weblayer {

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

WebLayerWebappsClient::WebLayerWebappsClient() = default;
WebLayerWebappsClient::~WebLayerWebappsClient() = default;

// static
void WebLayerWebappsClient::Create() {
  static base::NoDestructor<WebLayerWebappsClient> instance;
  instance.get();
}

bool WebLayerWebappsClient::IsOriginConsideredSecure(
    const url::Origin& origin) {
  return false;
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
  if (trigger == webapps::InstallTrigger::AMBIENT_BADGE) {
    // RICH_INSTALL_UI is the new name for AMBIENT_BADGE.
    return webapps::WebappInstallSource::RICH_INSTALL_UI_WEBLAYER;
  }
  return webapps::WebappInstallSource::COUNT;
}

webapps::AppBannerManager* WebLayerWebappsClient::GetAppBannerManager(
    content::WebContents* web_contents) {
  return WebLayerAppBannerManagerAndroid::FromWebContents(web_contents);
}

bool WebLayerWebappsClient::IsInstallationInProgress(
    content::WebContents* web_contents,
    const GURL& manifest_url,
    const GURL& manifest_id) {
  if (base::FeatureList::IsEnabled(webapps::features::kWebApkUniqueId))
    return current_install_ids_.count(manifest_id);
  return current_installs_.count(manifest_url) > 0;
}

bool WebLayerWebappsClient::CanShowAppBanners(
    content::WebContents* web_contents) {
  return WebApkInstallScheduler::IsInstallServiceAvailable();
}

void WebLayerWebappsClient::OnWebApkInstallInitiatedFromAppMenu(
    content::WebContents* web_contents) {}

void WebLayerWebappsClient::InstallWebApk(
    content::WebContents* web_contents,
    const webapps::AddToHomescreenParams& params) {
  DCHECK(current_installs_.count(params.shortcut_info->manifest_url) == 0);
  current_installs_.insert(params.shortcut_info->manifest_url);
  current_install_ids_.insert(params.shortcut_info->manifest_id);
  WebApkInstallScheduler::FetchProtoAndScheduleInstall(
      web_contents, *(params.shortcut_info), params.primary_icon,
      params.has_maskable_primary_icon,
      base::BindOnce(&WebLayerWebappsClient::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebLayerWebappsClient::InstallShortcut(
    content::WebContents* web_contents,
    const webapps::AddToHomescreenParams& params) {
  const webapps::ShortcutInfo& info = *params.shortcut_info;

  webapps::addShortcutToHomescreen(base::GenerateGUID(), info.url,
                                   info.user_title, params.primary_icon,
                                   params.has_maskable_primary_icon);
}

void WebLayerWebappsClient::OnInstallFinished(GURL manifest_url,
                                              GURL manifest_id) {
  DCHECK(current_installs_.count(manifest_url) == 1);
  current_installs_.erase(manifest_url);
  current_install_ids_.erase(manifest_id);
}

}  // namespace weblayer

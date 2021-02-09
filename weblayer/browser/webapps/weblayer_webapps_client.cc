// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/weblayer_webapps_client.h"

#include "base/logging.h"
#include "components/security_state/content/content_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "weblayer/browser/infobar_service.h"

namespace weblayer {

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
  return InfoBarService::FromWebContents(web_contents);
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
  NOTIMPLEMENTED();
}
#endif

}  // namespace weblayer

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_WEBAPPS_CLIENT_H_
#define WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_WEBAPPS_CLIENT_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/webapps/browser/webapps_client.h"

namespace weblayer {

class WebLayerWebappsClient : public webapps::WebappsClient {
 public:
  WebLayerWebappsClient(const WebLayerWebappsClient&) = delete;
  WebLayerWebappsClient& operator=(const WebLayerWebappsClient&) = delete;

  static void Create();

  // WebappsClient:
  security_state::SecurityLevel GetSecurityLevelForWebContents(
      content::WebContents* web_contents) override;
  infobars::ContentInfoBarManager* GetInfoBarManagerForWebContents(
      content::WebContents* web_contents) override;
  webapps::WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      webapps::InstallTrigger trigger) override;
  webapps::AppBannerManager* GetAppBannerManager(
      content::WebContents* web_contents) override;
#if BUILDFLAG(IS_ANDROID)
  bool IsInstallationInProgress(content::WebContents* web_contents,
                                const GURL& manifest_url) override;
  bool CanShowAppBanners(content::WebContents* web_contents) override;
  void OnWebApkInstallInitiatedFromAppMenu(
      content::WebContents* web_contents) override;
  void InstallWebApk(content::WebContents* web_contents,
                     const webapps::AddToHomescreenParams& params) override;
  void InstallShortcut(content::WebContents* web_contents,
                       const webapps::AddToHomescreenParams& params) override;
#endif

 private:
  friend base::NoDestructor<WebLayerWebappsClient>;

  WebLayerWebappsClient() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_WEBAPPS_CLIENT_H_

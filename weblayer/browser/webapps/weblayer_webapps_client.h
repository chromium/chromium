// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_WEBAPPS_CLIENT_H_
#define WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_WEBAPPS_CLIENT_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/webapps/browser/webapps_client.h"

class GURL;

namespace url {
class Origin;
}

namespace weblayer {

class WebLayerWebappsClient : public webapps::WebappsClient {
 public:
  // Called when the scheduling of an WebAPK installation with the Chrome
  // service finished or failed.
  using WebApkInstallFinishedCallback = base::OnceCallback<void(GURL, GURL)>;

  WebLayerWebappsClient(const WebLayerWebappsClient&) = delete;
  WebLayerWebappsClient& operator=(const WebLayerWebappsClient&) = delete;

  static void Create();

  // WebappsClient:
  bool IsOriginConsideredSecure(const url::Origin& origin) override;
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
                                const GURL& manifest_url,
                                const GURL& manifest_id) override;
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

  WebLayerWebappsClient();
  ~WebLayerWebappsClient() override;

  void OnInstallFinished(GURL manifest_url, GURL manifest_id);

  std::set<GURL> current_installs_;
  std::set<GURL> current_install_ids_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebLayerWebappsClient> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_WEBAPPS_CLIENT_H_

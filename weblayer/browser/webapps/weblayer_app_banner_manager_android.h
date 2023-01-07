// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_APP_BANNER_MANAGER_ANDROID_H_
#define WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_APP_BANNER_MANAGER_ANDROID_H_

#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "content/public/browser/web_contents_user_data.h"

namespace weblayer {

class WebLayerAppBannerManagerAndroid
    : public webapps::AppBannerManagerAndroid,
      public content::WebContentsUserData<WebLayerAppBannerManagerAndroid> {
 public:
  explicit WebLayerAppBannerManagerAndroid(content::WebContents* web_contents);
  WebLayerAppBannerManagerAndroid(const WebLayerAppBannerManagerAndroid&) =
      delete;
  WebLayerAppBannerManagerAndroid& operator=(
      const WebLayerAppBannerManagerAndroid&) = delete;
  ~WebLayerAppBannerManagerAndroid() override;

  using content::WebContentsUserData<
      WebLayerAppBannerManagerAndroid>::FromWebContents;

 protected:
  webapps::InstallableParams ParamsToPerformInstallableWebAppCheck() override;
  void ShowAmbientBadge() override;
  void ShowBannerUi(webapps::WebappInstallSource install_source) override;

 private:
  friend class content::WebContentsUserData<WebLayerAppBannerManagerAndroid>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBAPPS_WEBLAYER_APP_BANNER_MANAGER_ANDROID_H_

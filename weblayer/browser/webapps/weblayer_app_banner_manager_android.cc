// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/weblayer_app_banner_manager_android.h"

#include "components/webapps/browser/android/app_banner_manager_android.h"

namespace weblayer {

WebLayerAppBannerManagerAndroid::WebLayerAppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManagerAndroid(web_contents),
      content::WebContentsUserData<WebLayerAppBannerManagerAndroid>(
          *web_contents) {}

WebLayerAppBannerManagerAndroid::~WebLayerAppBannerManagerAndroid() = default;

webapps::InstallableParams
WebLayerAppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck() {
  webapps::InstallableParams params =
      AppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck();
  params.fetch_screenshots = true;
  return params;
}

void WebLayerAppBannerManagerAndroid::ShowAmbientBadge() {
  webapps::WebappInstallSource install_source =
      webapps::InstallableMetrics::GetInstallSource(
          web_contents(), webapps::InstallTrigger::AMBIENT_BADGE);
  if (!MaybeShowPwaBottomSheetController(/* expand_sheet= */ false,
                                         install_source)) {
    AppBannerManagerAndroid::ShowAmbientBadge();
  }
}

void WebLayerAppBannerManagerAndroid::ShowBannerUi(
    webapps::WebappInstallSource install_source) {
  if (!native_app_data_.is_null()) {
    AppBannerManagerAndroid::ShowBannerUi(install_source);
    return;
  }

  if (!MaybeShowPwaBottomSheetController(/* expand_sheet= */ true,
                                         install_source)) {
    AppBannerManagerAndroid::ShowBannerUi(install_source);
    return;
  }

  ReportStatus(webapps::SHOWING_WEB_APP_BANNER);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebLayerAppBannerManagerAndroid);

}  // namespace weblayer

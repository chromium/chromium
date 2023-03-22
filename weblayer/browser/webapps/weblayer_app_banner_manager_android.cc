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

void WebLayerAppBannerManagerAndroid::MaybeShowAmbientBadge() {
  // TODO(crbug/1420605): Enable WebApk install BottomSheet/Banner for WebEngine
  // sandbox mode.
}

void WebLayerAppBannerManagerAndroid::ShowBannerUi(
    webapps::WebappInstallSource install_source) {
  // TODO(crbug/1420605): Enable WebApk install BottomSheet/Banner for WebEngine
  // sandbox mode.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebLayerAppBannerManagerAndroid);

}  // namespace weblayer

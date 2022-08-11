// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_PERMISSIONS_CLIENT_H_
#define WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_PERMISSIONS_CLIENT_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/permissions/permissions_client.h"

namespace weblayer {

class WebLayerPermissionsClient : public permissions::PermissionsClient {
 public:
  WebLayerPermissionsClient(const WebLayerPermissionsClient&) = delete;
  WebLayerPermissionsClient& operator=(const WebLayerPermissionsClient&) =
      delete;

  static WebLayerPermissionsClient* GetInstance();

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) override;
  bool IsSubresourceFilterActivated(content::BrowserContext* browser_context,
                                    const GURL& url) override;
  permissions::PermissionActionsHistory* GetPermissionActionsHistory(
      content::BrowserContext* browser_context) override;
  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  permissions::ObjectPermissionContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) override;
#if BUILDFLAG(IS_ANDROID)
  void RepromptForAndroidPermissions(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      PermissionsUpdatedCallback callback) override;
  int MapToJavaDrawableId(int resource_id) override;
#endif

 private:
  friend base::NoDestructor<WebLayerPermissionsClient>;

  WebLayerPermissionsClient() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_PERMISSIONS_CLIENT_H_

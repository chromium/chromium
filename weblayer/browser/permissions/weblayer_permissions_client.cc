// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/weblayer_permissions_client.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/origin_keyed_permission_action_service_factory.h"
#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "weblayer/browser/subresource_filter_profile_context_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "weblayer/browser/android/permission_request_utils.h"
#include "weblayer/browser/android/resource_mapper.h"
#endif

namespace weblayer {

// static
WebLayerPermissionsClient* WebLayerPermissionsClient::GetInstance() {
  static base::NoDestructor<WebLayerPermissionsClient> instance;
  return instance.get();
}

HostContentSettingsMap* WebLayerPermissionsClient::GetSettingsMap(
    content::BrowserContext* browser_context) {
  return HostContentSettingsMapFactory::GetForBrowserContext(browser_context);
}

scoped_refptr<content_settings::CookieSettings>
WebLayerPermissionsClient::GetCookieSettings(
    content::BrowserContext* browser_context) {
  return CookieSettingsFactory::GetForBrowserContext(browser_context);
}

bool WebLayerPermissionsClient::IsSubresourceFilterActivated(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return SubresourceFilterProfileContextFactory::GetForBrowserContext(
             browser_context)
      ->settings_manager()
      ->GetSiteActivationFromMetadata(url);
}

permissions::OriginKeyedPermissionActionService*
WebLayerPermissionsClient::GetOriginKeyedPermissionActionService(
    content::BrowserContext* browser_context) {
  return OriginKeyedPermissionActionServiceFactory::GetForBrowserContext(
      browser_context);
}

permissions::PermissionDecisionAutoBlocker*
WebLayerPermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return PermissionDecisionAutoBlockerFactory::GetForBrowserContext(
      browser_context);
}

// PermissionActionsHistory would never be read in WebLayer, so it seems logical
// not to have the service at all.
permissions::PermissionActionsHistory*
WebLayerPermissionsClient::GetPermissionActionsHistory(
    content::BrowserContext* browser_context) {
  return nullptr;
}

permissions::ObjectPermissionContextBase*
WebLayerPermissionsClient::GetChooserContext(
    content::BrowserContext* browser_context,
    ContentSettingsType type) {
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
void WebLayerPermissionsClient::RepromptForAndroidPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    const std::vector<ContentSettingsType>& filtered_content_settings_types,
    const std::vector<std::string>& required_permissions,
    const std::vector<std::string>& optional_permissions,
    PermissionsUpdatedCallback callback) {
  RequestAndroidPermissions(web_contents, content_settings_types,
                            std::move(callback));
}

int WebLayerPermissionsClient::MapToJavaDrawableId(int resource_id) {
  return weblayer::MapToJavaDrawableId(resource_id);
}
#endif

}  // namespace weblayer

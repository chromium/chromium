// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/background_fetch/background_fetch_permission_context.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

BackgroundFetchPermissionContext::BackgroundFetchPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::BACKGROUND_FETCH,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

ContentSetting BackgroundFetchPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // Follow the AUTOMATIC_DOWNLOADS setting. TODO(crbug.com/1189247): can this
  // be improved upon? It's not really "automatic" if it's in direct response to
  // a user action, but WebLayer doesn't implement Chrome's download request
  // limiting logic.
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForBrowserContext(browser_context());
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      requesting_origin, requesting_origin,
      ContentSettingsType::AUTOMATIC_DOWNLOADS);

  // Matching Chrome behavior: when the request originates from a non-main frame
  // or a service worker, the most permissive we'll allow is ASK. This causes
  // the download to start in a paused state.
  if (setting == CONTENT_SETTING_ALLOW &&
      (!render_frame_host || render_frame_host->GetParent())) {
    setting = CONTENT_SETTING_ASK;
  }

  return setting;
}

}  // namespace weblayer

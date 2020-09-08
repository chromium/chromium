// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/default_search_engine.h"

#include "base/no_destructor.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

const url::Origin& GetDseOrigin() {
  static const base::NoDestructor<url::Origin> kOrigin(
      url::Origin::Create(GURL("https://www.google.com")));
  return *kOrigin;
}

bool IsPermissionControlledByDse(ContentSettingsType type,
                                 const url::Origin& origin) {
  return type == ContentSettingsType::GEOLOCATION && GetDseOrigin() == origin;
}

void ResetDsePermissions(content::BrowserContext* browser_context) {
  // Incognito should still have to prompt for permissions.
  if (browser_context->IsOffTheRecord())
    return;
  GURL url = GetDseOrigin().GetURL();
  HostContentSettingsMapFactory::GetForBrowserContext(browser_context)
      ->SetContentSettingDefaultScope(url, url,
                                      ContentSettingsType::GEOLOCATION,
                                      std::string(), CONTENT_SETTING_ALLOW);
}

}  // namespace weblayer

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DEFAULT_SEARCH_ENGINE_H_
#define WEBLAYER_BROWSER_DEFAULT_SEARCH_ENGINE_H_

#include "components/content_settings/core/common/content_settings.h"

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

namespace weblayer {
const url::Origin& GetDseOrigin();

// Returns whether permissions for |type| and |origin| are controlled by
// WebLayer's default search engine logic. This only applies to the GEOLOCATION
// permission, which will be force allowed and controlled by the client app's
// system level location permissions.
bool IsPermissionControlledByDse(ContentSettingsType type,
                                 const url::Origin& origin);

// Resets all permissions managed by WebLayer for the default search engine.
// TODO(crbug.com/1063433): If this logic gets more complicated consider
// refactoring SearchPermissionsService to be used in WebLayer.
void ResetDsePermissions(content::BrowserContext* browser_context);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DEFAULT_SEARCH_ENGINE_H_

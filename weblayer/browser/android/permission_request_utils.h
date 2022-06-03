// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_PERMISSION_REQUEST_UTILS_H_
#define WEBLAYER_BROWSER_ANDROID_PERMISSION_REQUEST_UTILS_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}

namespace weblayer {

using PermissionsUpdatedCallback = base::OnceCallback<void(bool)>;

// Requests all necessary Android permissions related to
// |content_settings_types|, and calls |callback|. |callback| will be called
// with true if all permissions were successfully granted, and false otherwise.
void RequestAndroidPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_type,
    PermissionsUpdatedCallback callback);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_PERMISSION_REQUEST_UTILS_H_

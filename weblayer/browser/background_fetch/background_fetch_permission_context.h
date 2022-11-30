// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_PERMISSION_CONTEXT_H_
#define WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_context_base.h"

class GURL;

namespace weblayer {

// Manages user permissions for Background Fetch. Background Fetch permission
// is currently dynamic and relies on the Automatic Downloads content setting.
class BackgroundFetchPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit BackgroundFetchPermissionContext(
      content::BrowserContext* browser_context);
  BackgroundFetchPermissionContext(
      const BackgroundFetchPermissionContext& other) = delete;
  BackgroundFetchPermissionContext& operator=(
      const BackgroundFetchPermissionContext& other) = delete;
  ~BackgroundFetchPermissionContext() override = default;

 private:
  // PermissionContextBase implementation.
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_PERMISSION_CONTEXT_H_

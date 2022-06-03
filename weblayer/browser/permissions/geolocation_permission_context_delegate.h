// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERMISSIONS_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_H_
#define WEBLAYER_BROWSER_PERMISSIONS_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_H_

#include "build/build_config.h"
#include "components/permissions/contexts/geolocation_permission_context.h"

namespace weblayer {

class GeolocationPermissionContextDelegate
    : public permissions::GeolocationPermissionContext::Delegate {
 public:
  GeolocationPermissionContextDelegate() = default;

  GeolocationPermissionContextDelegate(
      const GeolocationPermissionContextDelegate&) = delete;
  GeolocationPermissionContextDelegate& operator=(
      const GeolocationPermissionContextDelegate&) = delete;

  // GeolocationPermissionContext::Delegate:
  bool DecidePermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback* callback,
      permissions::GeolocationPermissionContext* context) override;
#if defined(OS_ANDROID)
  bool IsInteractable(content::WebContents* web_contents) override;
  PrefService* GetPrefs(content::BrowserContext* browser_context) override;
  bool IsRequestingOriginDSE(content::BrowserContext* browser_context,
                             const GURL& requesting_origin) override;
  void FinishNotifyPermissionSet(const permissions::PermissionRequestID& id,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin) override;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERMISSIONS_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_H_

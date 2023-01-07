// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/weblayer_camera_pan_tilt_zoom_permission_context_delegate.h"

#include "build/build_config.h"

namespace weblayer {

WebLayerCameraPanTiltZoomPermissionContextDelegate::
    WebLayerCameraPanTiltZoomPermissionContextDelegate() = default;

WebLayerCameraPanTiltZoomPermissionContextDelegate::
    ~WebLayerCameraPanTiltZoomPermissionContextDelegate() = default;

bool WebLayerCameraPanTiltZoomPermissionContextDelegate::
    GetPermissionStatusInternal(const GURL& requesting_origin,
                                const GURL& embedding_origin,
                                ContentSetting* content_setting_result) {
#if BUILDFLAG(IS_ANDROID)
  // The PTZ permission is automatically granted on Android. It is safe to do so
  // because pan and tilt are not supported on Android.
  *content_setting_result = CONTENT_SETTING_ALLOW;
  return true;
#else
  return false;
#endif
}

}  // namespace weblayer

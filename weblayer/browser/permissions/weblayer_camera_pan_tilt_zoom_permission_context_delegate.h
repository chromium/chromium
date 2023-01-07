// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_DELEGATE_H_
#define WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_DELEGATE_H_

#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"

namespace weblayer {

class WebLayerCameraPanTiltZoomPermissionContextDelegate
    : public permissions::CameraPanTiltZoomPermissionContext::Delegate {
 public:
  WebLayerCameraPanTiltZoomPermissionContextDelegate();

  WebLayerCameraPanTiltZoomPermissionContextDelegate(
      const WebLayerCameraPanTiltZoomPermissionContextDelegate&) = delete;
  WebLayerCameraPanTiltZoomPermissionContextDelegate& operator=(
      const WebLayerCameraPanTiltZoomPermissionContextDelegate&) = delete;

  ~WebLayerCameraPanTiltZoomPermissionContextDelegate() override;

  // CameraPanTiltZoomPermissionContext::Delegate:
  bool GetPermissionStatusInternal(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      ContentSetting* content_setting_result) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_DELEGATE_H_
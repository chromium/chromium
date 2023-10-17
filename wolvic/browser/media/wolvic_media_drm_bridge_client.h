// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_MEDIA_WOLVIC_MEDIA_DRM_BRIDGE_CLIENT_H_
#define WOLVIC_BROWSER_MEDIA_WOLVIC_MEDIA_DRM_BRIDGE_CLIENT_H_

#include "components/cdm/common/clearkey_drm_delegate_android.h"
#include "components/cdm/common/widevine_drm_delegate_android.h"
#include "media/base/android/media_drm_bridge_client.h"

namespace wolvic {

class WolvicMediaDrmBridgeClient : public media::MediaDrmBridgeClient {
 public:
  WolvicMediaDrmBridgeClient() = default;
  WolvicMediaDrmBridgeClient(const WolvicMediaDrmBridgeClient&) = delete;
  WolvicMediaDrmBridgeClient& operator=(const WolvicMediaDrmBridgeClient&) =
      delete;
  ~WolvicMediaDrmBridgeClient() override = default;

 private:
  media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const media::UUID& scheme_uuid) override;

  cdm::WidevineDrmDelegateAndroid widevine_delegate_;
  cdm::ClearKeyDrmDelegateAndroid clearkey_delegate_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_MEDIA_WOLVIC_MEDIA_DRM_BRIDGE_CLIENT_H_

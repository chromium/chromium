// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "wolvic/browser/media/wolvic_media_drm_bridge_client.h"

#include "base/logging.h"

namespace wolvic {

media::MediaDrmBridgeDelegate*
WolvicMediaDrmBridgeClient::GetMediaDrmBridgeDelegate(
    const media::UUID& scheme_uuid) {
  if (scheme_uuid == widevine_delegate_.GetUUID()) {
    return &widevine_delegate_;
  }
  if (scheme_uuid == clearkey_delegate_.GetUUID()) {
    return &clearkey_delegate_;
  }
  return nullptr;
}

}  // namespace wolvic

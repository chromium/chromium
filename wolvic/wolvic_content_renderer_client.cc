// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_content_renderer_client.h"

#include "components/cdm/renderer/key_system_support_update.h"

namespace wolvic {

void WolvicContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  cdm::GetSupportedKeySystemsUpdates(/*can_persist_data=*/true, std::move(cb));
}

}  // namespace wolvic

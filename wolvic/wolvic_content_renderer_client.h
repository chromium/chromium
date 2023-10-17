// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

namespace wolvic {

class WolvicContentRendererClient : public content::ContentRendererClient {
 public:
  WolvicContentRendererClient() = default;

  WolvicContentRendererClient(const WolvicContentRendererClient&) = delete;
  WolvicContentRendererClient& operator=(const WolvicContentRendererClient&) =
      delete;

  ~WolvicContentRendererClient() override = default;

  // ContentRendererClient implementation.
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
};

}  // namespace wolvic

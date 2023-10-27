// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/renderer/wolvic_content_renderer_client.h"

#include "components/cdm/renderer/key_system_support_update.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"

namespace wolvic {

WolvicContentRendererClient::WolvicContentRendererClient() = default;

WolvicContentRendererClient::~WolvicContentRendererClient() = default;

void WolvicContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  cdm::GetSupportedKeySystemsUpdates(/*can_persist_data=*/true, std::move(cb));
}

void WolvicContentRendererClient::RenderThreadStarted() {
  visited_link_reader_ = std::make_unique<visitedlink::VisitedLinkReader>();
}

uint64_t WolvicContentRendererClient::VisitedLinkHash(const char* canonical_url,
						      size_t length) {
  return visited_link_reader_->ComputeURLFingerprint(canonical_url, length);
}

bool WolvicContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return visited_link_reader_->IsVisited(link_hash);
}

}  // namespace wolvic

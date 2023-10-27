// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

namespace visitedlink {
class VisitedLinkReader;
}

namespace wolvic {

class WolvicContentRendererClient : public content::ContentRendererClient {
 public:
  WolvicContentRendererClient();

  WolvicContentRendererClient(const WolvicContentRendererClient&) = delete;
  WolvicContentRendererClient& operator=(const WolvicContentRendererClient&) =
      delete;

  ~WolvicContentRendererClient() override;

  // ContentRendererClient implementation.
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
  void RenderThreadStarted() override;

  uint64_t VisitedLinkHash(const char* canonical_url, size_t length) override;
  bool IsLinkVisited(uint64_t link_hash) override;

  visitedlink::VisitedLinkReader* visited_link_reader() {
    return visited_link_reader_.get();
  }

private:
  std::unique_ptr<visitedlink::VisitedLinkReader> visited_link_reader_;
};

}  // namespace wolvic

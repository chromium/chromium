// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef WOLVIC_RENDERER_WOLVIC_CONTENT_RENDERER_CLIENT_H_
#define WOLVIC_RENDERER_WOLVIC_CONTENT_RENDERER_CLIENT_H_

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
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;

  uint64_t VisitedLinkHash(std::string_view canonical_url) override;
  bool IsLinkVisited(uint64_t link_hash) override;

  visitedlink::VisitedLinkReader* visited_link_reader() {
    return visited_link_reader_.get();
  }

private:
  std::unique_ptr<visitedlink::VisitedLinkReader> visited_link_reader_;
};

}  // namespace wolvic

#endif  // WOLVIC_RENDERER_WOLVIC_CONTENT_RENDERER_CLIENT_H_
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define UI_WEBUI_EXAMPLES_RENDERER_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

namespace webui_examples {

class ContentRendererClient : public content::ContentRendererClient {
 public:
  ContentRendererClient();
  ContentRendererClient(const ContentRendererClient&) = delete;
  ContentRendererClient& operator=(const ContentRendererClient&) = delete;
  ~ContentRendererClient() override;

 private:
  // content::ContentRendererClient:
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderThreadStarted() override;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_RENDERER_CONTENT_RENDERER_CLIENT_H_

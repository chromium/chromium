// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_
#define WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"

namespace weblayer {

class ContentRendererClientImpl : public content::ContentRendererClient {
 public:
  ContentRendererClientImpl();
  ~ContentRendererClientImpl() override;

  // content::ContentRendererClient:
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  bool HasErrorPage(int http_status_code) override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        std::string* error_html) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentRendererClientImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/renderer/content_renderer_client.h"

#include "components/secure_embed/buildflags/buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "ui/webui/examples/renderer/render_frame_observer.h"

#if BUILDFLAG(ENABLE_SECURE_EMBED)
#include "components/secure_embed/renderer/create_plugin.h"
#endif  // BUILDFLAG(ENABLE_SECURE_EMBED)

namespace webui_examples {

ContentRendererClient::ContentRendererClient() = default;

ContentRendererClient::~ContentRendererClient() = default;

void ContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  if (!render_frame->IsMainFrame()) {
    return;
  }

  std::unique_ptr<RenderFrameObserver> observer =
      std::make_unique<RenderFrameObserver>(render_frame);
  RenderFrameObserver* observer_ptr = observer.get();
  observer_ptr->SelfOwn(std::move(observer));
}

void ContentRendererClient::RenderThreadStarted() {
  blink::WebCustomElement::AddEmbedderCustomElementName(
      blink::WebString("webview"));
  blink::WebCustomElement::AddEmbedderCustomElementName(
      blink::WebString("secureembed"));
}

#if BUILDFLAG(ENABLE_SECURE_EMBED)
bool ContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  if (secure_embed::MaybeCreatePlugin(render_frame, params, plugin)) {
    return true;
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_SECURE_EMBED)

}  // namespace webui_examples

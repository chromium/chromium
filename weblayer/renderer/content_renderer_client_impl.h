// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_
#define WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_

#include "build/build_config.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

class SpellCheck;

namespace service_manager {
class LocalInterfaceProvider;
}  // namespace service_manager

namespace subresource_filter {
class UnverifiedRulesetDealer;
}

namespace weblayer {
class WebLayerRenderThreadObserver;

class ContentRendererClientImpl : public content::ContentRendererClient {
 public:
  ContentRendererClientImpl();

  ContentRendererClientImpl(const ContentRendererClientImpl&) = delete;
  ContentRendererClientImpl& operator=(const ContentRendererClientImpl&) =
      delete;

  ~ContentRendererClientImpl() override;

  // content::ContentRendererClient:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void WebViewCreated(blink::WebView* web_view,
                      bool was_created_by_renderer,
                      const url::Origin* outermost_origin) override;
  SkBitmap* GetSadPluginBitmap() override;
  SkBitmap* GetSadWebViewBitmap() override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        content::mojom::AlternativeErrorPageOverrideInfoPtr
                            alternative_error_page_info,
                        std::string* error_html) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  bool IsPrefetchOnly(content::RenderFrame* render_frame) override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      base::OnceClosure closure) override;

 private:
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<service_manager::LocalInterfaceProvider>
      local_interface_provider_;
  std::unique_ptr<SpellCheck> spellcheck_;
#endif

  std::unique_ptr<subresource_filter::UnverifiedRulesetDealer>
      subresource_filter_ruleset_dealer_;

  std::unique_ptr<WebLayerRenderThreadObserver> weblayer_observer_;

  scoped_refptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      browser_interface_broker_;
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/content_browser_client.h"

#include "components/embedder_support/user_agent_utils.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "components/secure_embed/buildflags/buildflags.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "ui/webui/examples/browser/browser_main_parts.h"
#include "ui/webui/examples/browser/ui/web/browser.h"
#include "ui/webui/examples/browser/ui/web/browser.mojom.h"

#if BUILDFLAG(ENABLE_SECURE_EMBED)
#include "components/secure_embed/browser/secure_embed_host.h"
#endif  // BUILDFLAG(ENABLE_SECURE_EMBED)

namespace webui_examples {

ContentBrowserClient::ContentBrowserClient() = default;

ContentBrowserClient::~ContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  auto browser_main_parts = BrowserMainParts::Create();
  browser_main_parts_ = browser_main_parts.get();
  return browser_main_parts;
}

std::unique_ptr<content::WebContentsViewDelegate>
ContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return browser_main_parts_->CreateWebContentsViewDelegate(web_contents);
}

std::unique_ptr<content::DevToolsManagerDelegate>
ContentBrowserClient::CreateDevToolsManagerDelegate() {
  return browser_main_parts_->CreateDevToolsManagerDelegate();
}

void ContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  RegisterWebUIControllerInterfaceBinder<
      webui_examples::mojom::PageHandlerFactory, Browser>(map);
  RegisterWebUIControllerInterfaceBinder<
      guest_contents::mojom::GuestContentsHost, Browser>(map);
}

#if BUILDFLAG(ENABLE_SECURE_EMBED)
void ContentBrowserClient::RegisterAssociatedInterfaceBindersForRenderFrameHost(
    content::RenderFrameHost& render_frame_host,
    blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<secure_embed::mojom::SecureEmbedHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 secure_embed::mojom::SecureEmbedHost> receiver) {
            secure_embed::SecureEmbedHost::Create(render_frame_host,
                                                  std::move(receiver));
          },
          &render_frame_host));
}
#endif  // BUILDFLAG(ENABLE_SECURE_EMBED)

std::string ContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

}  // namespace webui_examples

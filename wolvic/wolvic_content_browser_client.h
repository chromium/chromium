// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_
#define WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "wolvic/browser/dialogs/user_dialog_manager_bridge.h"
#include "wolvic/browser/vr/wolvic_xr_integration_client.h"

namespace content {
class BrowserContext;
}
namespace wolvic {

class WolvicMainParts;

class WolvicContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit WolvicContentBrowserClient();

  WolvicContentBrowserClient(const WolvicContentBrowserClient&) = delete;
  WolvicContentBrowserClient& operator=(const WolvicContentBrowserClient&) =
      delete;

  ~WolvicContentBrowserClient() override;

  // Returns the single instance.
  static WolvicContentBrowserClient* Get();

  content::BrowserContext* browser_context();
  content::BrowserContext* off_the_record_browser_context();

  // ContentBrowserClient overrides.
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const content::GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
#if BUILDFLAG(ENABLE_VR)
  content::XrIntegrationClient* GetXrIntegrationClient() override;
#endif
  void BindMediaServiceReceiver(content::RenderFrameHost* render_frame_host,
                                mojo::GenericPendingReceiver receiver) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;

 private:
  raw_ptr<WolvicMainParts> browser_main_parts_;
#if BUILDFLAG(ENABLE_VR)
  std::unique_ptr<WolvicXrIntegrationClient> xr_integration_client_;
#endif
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_

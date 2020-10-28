// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"

class PrefService;

namespace weblayer {

class FeatureListCreator;
class SafeBrowsingService;
struct MainParams;

blink::UserAgentMetadata GetUserAgentMetadata();

class ContentBrowserClientImpl : public content::ContentBrowserClient {
 public:
  explicit ContentBrowserClientImpl(MainParams* params);
  ~ContentBrowserClientImpl() override;

  // ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  std::string GetApplicationLocale() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  bool CanShutdownGpuProcessNowOnIOThread() override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  void LogWebFeatureForCurrentPage(content::RenderFrameHost* render_frame_host,
                                   blink::mojom::WebFeature feature) override;
  std::string GetProduct() override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                           blink::web_pref::WebPreferences* prefs) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      network::mojom::CertVerifierCreationParams* cert_verifier_creation_params)
      override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  bool IsHandledURL(const GURL& url) override;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override;
  bool MayReuseHost(content::RenderProcessHost* process_host) override;
  void OverridePageVisibilityState(
      content::RenderFrameHost* render_frame_host,
      content::PageVisibilityState* visibility_state) override;
  bool ShouldDisableSiteIsolation() override;
  std::vector<std::string> GetAdditionalSiteIsolationModes() override;
  void PersistIsolatedOrigin(content::BrowserContext* context,
                             const url::Origin& origin) override;
  base::OnceClosure SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  content::ControllerPresentationServiceDelegate*
  GetControllerPresentationServiceDelegate(
      content::WebContents* web_contents) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  bool BindAssociatedReceiverFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void BindMediaServiceReceiver(content::RenderFrameHost* render_frame_host,
                                mojo::GenericPendingReceiver receiver) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  void BindHostReceiverForRenderer(
      content::RenderProcessHost* render_process_host,
      mojo::GenericPendingReceiver receiver) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  scoped_refptr<content::QuotaPermissionContext> CreateQuotaPermissionContext()
      override;
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
#if defined(OS_ANDROID)
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      base::Optional<int64_t> navigation_id,
      base::UkmSourceId ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override) override;
  WideColorGamutHeuristic GetWideColorGamutHeuristic() override;
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  std::unique_ptr<content::TtsEnvironmentAndroid> CreateTtsEnvironmentAndroid()
      override;
#endif  // OS_ANDROID
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  ukm::UkmService* GetUkmService() override;

  void CreateFeatureListAndFieldTrials();

 private:
  std::unique_ptr<PrefService> CreateLocalState();

#if defined(OS_ANDROID)
  SafeBrowsingService* GetSafeBrowsingService();
#endif

  MainParams* params_;

  // Local-state is created early on, before BrowserProcess. Ownership moves to
  // BrowserMainParts, then BrowserProcess. BrowserProcess ultimately owns
  // local-state so that it can be destroyed along with other BrowserProcess
  // state.
  std::unique_ptr<PrefService> local_state_;

  std::unique_ptr<FeatureListCreator> feature_list_creator_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "device/vr/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"

class PrefService;

namespace blink {
class StorageKey;
}  // namespace blink

namespace net {
class SiteForCookies;
}  // namespace net

namespace permissions {
class BluetoothDelegateImpl;
}

namespace weblayer {

class FeatureListCreator;
class SafeBrowsingService;
struct MainParams;

#if BUILDFLAG(ENABLE_ARCORE)
class XrIntegrationClientImpl;
#endif  // BUILDFLAG(ENABLE_ARCORE)

class ContentBrowserClientImpl : public content::ContentBrowserClient {
 public:
  explicit ContentBrowserClientImpl(MainParams* params);
  ~ContentBrowserClientImpl() override;

  // ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::string GetApplicationLocale() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  content::AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context) override;
  bool AllowSharedWorker(const GURL& worker_url,
                         const net::SiteForCookies& site_for_cookies,
                         const absl::optional<url::Origin>& top_frame_origin,
                         const std::string& name,
                         const blink::StorageKey& storage_key,
                         content::BrowserContext* context,
                         int render_process_id,
                         int render_frame_id) override;
  void AllowWorkerFileSystem(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalRenderFrameHostId>& render_frames,
      base::OnceCallback<void(bool)> callback) override;
  bool AllowWorkerIndexedDB(const GURL& url,
                            content::BrowserContext* browser_context,
                            const std::vector<content::GlobalRenderFrameHostId>&
                                render_frames) override;
  bool AllowWorkerCacheStorage(
      const GURL& url,
      content::BrowserContext* browser_context,
      const std::vector<content::GlobalRenderFrameHostId>& render_frames)
      override;
  bool AllowWorkerWebLocks(const GURL& url,
                           content::BrowserContext* browser_context,
                           const std::vector<content::GlobalRenderFrameHostId>&
                               render_frames) override;
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  bool CanShutdownGpuProcessNowOnIOThread() override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  void LogWebFeatureForCurrentPage(content::RenderFrameHost* render_frame_host,
                                   blink::mojom::WebFeature feature) override;
  std::string GetProduct() override;
  std::string GetUserAgent() override;
  std::string GetFullUserAgent() override;
  std::string GetReducedUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
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
  bool ShouldDisableSiteIsolation(
      content::SiteIsolationMode site_isolation_mode) override;
  std::vector<std::string> GetAdditionalSiteIsolationModes() override;
  void PersistIsolatedOrigin(
      content::BrowserContext* context,
      const url::Origin& origin,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource source)
      override;
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
  void OpenURL(
      content::SiteInstance* site_instance,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::WebContents*)> callback) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
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
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) ||
        // BUILDFLAG(IS_ANDROID)
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
#if BUILDFLAG(IS_ANDROID)
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      absl::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
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
      bool is_request_for_primary_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  std::unique_ptr<content::TtsEnvironmentAndroid> CreateTtsEnvironmentAndroid()
      override;
  bool ShouldObserveContainerViewLocationForDialogOverlays() override;
  content::BluetoothDelegate* GetBluetoothDelegate() override;
#endif  // BUILDFLAG(IS_ANDROID)
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  bool ShouldSandboxNetworkService() override;
#if BUILDFLAG(ENABLE_ARCORE)
  content::XrIntegrationClient* GetXrIntegrationClient() override;
#endif  // BUILDFLAG(ENABLE_ARCORE)
  ukm::UkmService* GetUkmService() override;
  bool HasErrorPage(int http_status_code) override;
  bool IsClipboardPasteAllowed(
      content::RenderFrameHost* render_frame_host) override;
  bool ShouldPreconnectNavigation(
      content::BrowserContext* browser_context) override;

  void CreateFeatureListAndFieldTrials();

 private:
  std::unique_ptr<PrefService> CreateLocalState();

#if BUILDFLAG(IS_ANDROID)
  SafeBrowsingService* GetSafeBrowsingService();

  std::unique_ptr<permissions::BluetoothDelegateImpl> bluetooth_delegate_;
#endif

  raw_ptr<MainParams> params_;

  // Local-state is created early on, before BrowserProcess. Ownership moves to
  // BrowserMainParts, then BrowserProcess. BrowserProcess ultimately owns
  // local-state so that it can be destroyed along with other BrowserProcess
  // state.
  std::unique_ptr<PrefService> local_state_;

  std::unique_ptr<FeatureListCreator> feature_list_creator_;

#if BUILDFLAG(ENABLE_ARCORE)
  std::unique_ptr<XrIntegrationClientImpl> xr_integration_client_;
#endif  // BUILDFLAG(ENABLE_ARCORE)
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_

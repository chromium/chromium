// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_browser_client_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/embedder_support/content_settings_utils.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/metrics/metrics_service.h"
#include "components/network_time/network_time_tracker.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/prerender_url_loader_throttle.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/security_interstitials/content/insecure_form_navigation_throttle.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/site_isolation/pref_names.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_version.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/window_container_type.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/site_for_cookies.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_private_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "weblayer/browser/browser_main_parts_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/download_manager_delegate_impl.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/navigation_error_navigation_throttle.h"
#include "weblayer/browser/navigation_ui_data_impl.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_utils.h"
#include "weblayer/browser/page_specific_content_settings_delegate.h"
#include "weblayer/browser/password_manager_driver_factory.h"
#include "weblayer/browser/popup_navigation_delegate_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/signin_url_loader_throttle.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/web_contents_view_delegate_impl.h"
#include "weblayer/browser/weblayer_browser_interface_binders.h"
#include "weblayer/browser/weblayer_security_blocking_page_factory.h"
#include "weblayer/browser/weblayer_speech_recognition_manager_delegate.h"
#include "weblayer/common/features.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/fullscreen_delegate.h"
#include "weblayer/public/main.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/bundle_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/functional/bind.h"
#include "components/browser_ui/client_certificate/android/ssl_client_certificate_request.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"  // nogncheck
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/permissions/bluetooth_delegate_impl.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"  // nogncheck
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"  // nogncheck
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/spell_check_host_impl.h"  // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/android_descriptors.h"
#include "weblayer/browser/bluetooth/weblayer_bluetooth_delegate_impl_client.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/devtools_manager_delegate_android.h"
#include "weblayer/browser/http_auth_handler_impl.h"
#include "weblayer/browser/media/media_router_factory.h"
#include "weblayer/browser/proxying_url_loader_factory_impl.h"
#include "weblayer/browser/safe_browsing/real_time_url_lookup_service_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/tts_environment_android_impl.h"
#include "weblayer/browser/weblayer_factory_impl_android.h"
#endif

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_ANDROID)
#include "content/public/common/content_descriptors.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "weblayer/browser/captive_portal_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_ARCORE)
#include "weblayer/browser/xr/xr_integration_client_impl.h"
#endif  // BUILDFLAG(ENABLE_ARCORE)

namespace switches {
// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
// TODO(alexclarke): Find a better place for this.
const char kProxyBypassList[] = "proxy-bypass-list";
}  // namespace switches

namespace weblayer {

namespace {

bool IsSafebrowsingSupported() {
  // TODO(timvolodine): consider the non-android case, see crbug.com/1015809.
  // TODO(timvolodine): consider refactoring this out into safe_browsing/.
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

bool IsNetworkErrorAutoReloadEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(embedder_support::kEnableAutoReload))
    return true;
  if (command_line.HasSwitch(embedder_support::kDisableAutoReload))
    return false;
  return true;
}

bool IsInHostedApp(content::WebContents* web_contents) {
  return false;
}

bool ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps(
    content::NavigationHandle* handle) {
  return false;
}

class SSLCertReporterImpl : public SSLCertReporter {
 public:
  void ReportInvalidCertificateChain(
      const std::string& serialized_report) override {}
};

// Wrapper for SSLErrorHandler::HandleSSLError() that supplies //weblayer-level
// parameters.
void HandleSSLErrorWrapper(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback) {
  captive_portal::CaptivePortalService* captive_portal_service = nullptr;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal_service = CaptivePortalServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
#endif

  SSLErrorHandler::HandleSSLError(
      web_contents, cert_error, ssl_info, request_url,
      std::move(ssl_cert_reporter), std::move(blocking_page_ready_callback),
      BrowserProcess::GetInstance()->GetNetworkTimeTracker(),
      captive_portal_service,
      std::make_unique<WebLayerSecurityBlockingPageFactory>());
}

#if BUILDFLAG(IS_ANDROID)
void CreateOriginId(cdm::MediaDrmStorageImpl::OriginIdObtainedCB callback) {
  std::move(callback).Run(true, base::UnguessableToken::Create());
}

void AllowEmptyOriginIdCB(base::OnceCallback<void(bool)> callback) {
  // Since CreateOriginId() always returns a non-empty origin ID, we don't need
  // to allow empty origin ID.
  std::move(callback).Run(false);
}

void CreateMediaDrmStorage(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<::media::mojom::MediaDrmStorage> receiver) {
  CHECK(render_frame_host);

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    DVLOG(1) << __func__ << ": Unique origin.";
    return;
  }

  // The object will be deleted on connection error, or when the frame navigates
  // away.
  new cdm::MediaDrmStorageImpl(
      *render_frame_host, base::BindRepeating(&CreateOriginId),
      base::BindRepeating(&AllowEmptyOriginIdCB), std::move(receiver));
}
#endif  // BUILDFLAG(IS_ANDROID)

void RegisterPrefs(PrefRegistrySimple* pref_registry) {
  network_time::NetworkTimeTracker::RegisterPrefs(pref_registry);
  pref_registry->RegisterIntegerPref(kDownloadNextIDPref, 0);
#if BUILDFLAG(IS_ANDROID)
  metrics::AndroidMetricsServiceClient::RegisterPrefs(pref_registry);
  safe_browsing::RegisterLocalStatePrefs(pref_registry);
#else
  // Call MetricsService::RegisterPrefs() as VariationsService::RegisterPrefs()
  // CHECKs that kVariationsCrashStreak has already been registered.
  //
  // Note that the above call to AndroidMetricsServiceClient::RegisterPrefs()
  // implicitly calls MetricsService::RegisterPrefs(), so the below call is
  // necessary only on non-Android platforms.
  metrics::MetricsService::RegisterPrefs(pref_registry);
#endif
  variations::VariationsService::RegisterPrefs(pref_registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(pref_registry);
}

mojo::PendingRemote<prerender::mojom::PrerenderCanceler> GetPrerenderCanceler(
    content::WebContents* web_contents) {
  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler;
  weblayer::NoStatePrefetchContentsFromWebContents(web_contents)
      ->AddPrerenderCancelerReceiver(canceler.InitWithNewPipeAndPassReceiver());
  return canceler;
}

}  // namespace

ContentBrowserClientImpl::ContentBrowserClientImpl(MainParams* params)
    : params_(params) {
}

ContentBrowserClientImpl::~ContentBrowserClientImpl() = default;

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClientImpl::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  // This should be called after CreateFeatureListAndFieldTrials(), which
  // creates |local_state_|.
  DCHECK(local_state_);
  std::unique_ptr<BrowserMainPartsImpl> browser_main_parts =
      std::make_unique<BrowserMainPartsImpl>(params_, std::move(local_state_));

  return browser_main_parts;
}

std::string ContentBrowserClientImpl::GetApplicationLocale() {
  return i18n::GetApplicationLocale();
}

std::string ContentBrowserClientImpl::GetAcceptLangs(
    content::BrowserContext* context) {
  return i18n::GetAcceptLangs();
}

content::AllowServiceWorkerResult ContentBrowserClientImpl::AllowServiceWorker(
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return embedder_support::AllowServiceWorker(
      scope, site_for_cookies, top_frame_origin,
      CookieSettingsFactory::GetForBrowserContext(context).get(),
      HostContentSettingsMapFactory::GetForBrowserContext(context));
}

bool ContentBrowserClientImpl::AllowSharedWorker(
    const GURL& worker_url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
    content::BrowserContext* context,
    int render_process_id,
    int render_frame_id) {
  return embedder_support::AllowSharedWorker(
      worker_url, site_for_cookies, top_frame_origin, name, storage_key,
      render_process_id, render_frame_id,
      CookieSettingsFactory::GetForBrowserContext(context).get());
}

void ContentBrowserClientImpl::AllowWorkerFileSystem(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(embedder_support::AllowWorkerFileSystem(
      url, render_frames,
      CookieSettingsFactory::GetForBrowserContext(browser_context).get()));
}

bool ContentBrowserClientImpl::AllowWorkerIndexedDB(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  return embedder_support::AllowWorkerIndexedDB(
      url, render_frames,
      CookieSettingsFactory::GetForBrowserContext(browser_context).get());
}

bool ContentBrowserClientImpl::AllowWorkerCacheStorage(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  return embedder_support::AllowWorkerCacheStorage(
      url, render_frames,
      CookieSettingsFactory::GetForBrowserContext(browser_context).get());
}

bool ContentBrowserClientImpl::AllowWorkerWebLocks(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  return embedder_support::AllowWorkerWebLocks(
      url, CookieSettingsFactory::GetForBrowserContext(browser_context).get());
}

std::unique_ptr<content::WebContentsViewDelegate>
ContentBrowserClientImpl::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->MaybeCreatePageNodeForWebContents(web_contents);
  return std::make_unique<WebContentsViewDelegateImpl>(web_contents);
}

bool ContentBrowserClientImpl::CanShutdownGpuProcessNowOnIOThread() {
  return true;
}

std::unique_ptr<content::DevToolsManagerDelegate>
ContentBrowserClientImpl::CreateDevToolsManagerDelegate() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DevToolsManagerDelegateAndroid>();
#else
  return std::make_unique<content::DevToolsManagerDelegate>();
#endif
}

void ContentBrowserClientImpl::LogWebFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, feature);
}

std::string ContentBrowserClientImpl::GetProduct() {
  return std::string(version_info::GetProductNameAndVersionForUserAgent());
}

std::string ContentBrowserClientImpl::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

blink::UserAgentMetadata ContentBrowserClientImpl::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata();
}

void ContentBrowserClientImpl::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  prefs->default_encoding = l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING);
  // TODO(crbug.com/1131016): Support Picture in Picture on WebLayer.
  prefs->picture_in_picture_enabled = false;

  TabImpl* tab = TabImpl::FromWebContents(web_contents);
  if (tab)
    tab->SetWebPreferences(prefs);
}

void ContentBrowserClientImpl::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  SystemNetworkContextManager::ConfigureDefaultNetworkContextParams(
      context_params, GetUserAgent());
  // Headers coming from the embedder are implicitly trusted and should not
  // trigger CORS checks.
  context_params->allow_any_cors_exempt_header_for_browser = true;
  context_params->accept_language = GetAcceptLangs(context);
  if (!context->IsOffTheRecord()) {
    context_params->file_paths = network::mojom::NetworkContextFilePaths::New();
    context_params->file_paths->data_directory = context->GetPath();
    context_params->file_paths->cookie_database_name =
        base::FilePath(FILE_PATH_LITERAL("Cookies"));
    context_params->file_paths->http_cache_directory =
        ProfileImpl::GetCachePath(context).Append(FILE_PATH_LITERAL("Cache"));
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kProxyServer)) {
    std::string proxy_server =
        command_line->GetSwitchValueASCII(::switches::kProxyServer);
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy_server);
    if (command_line->HasSwitch(::switches::kProxyBypassList)) {
      std::string bypass_list =
          command_line->GetSwitchValueASCII(::switches::kProxyBypassList);
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        proxy_config,
        net::DefineNetworkTrafficAnnotation("undefined", "Nothing here yet."));
  }
  if (command_line->HasSwitch(embedder_support::kShortReportingDelay)) {
    context_params->reporting_delivery_interval = base::Milliseconds(100);
  }
}

void ContentBrowserClientImpl::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(
        embedder_support::GetUserAgent());
  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);
}

std::unique_ptr<blink::URLLoaderThrottle>
ContentBrowserClientImpl::MaybeCreateSafeBrowsingURLLoaderThrottle(
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    int frame_tree_node_id) {
  if (base::FeatureList::IsEnabled(features::kWebLayerSafeBrowsing) &&
      IsSafebrowsingSupported()) {
#if BUILDFLAG(IS_ANDROID)
    BrowserContextImpl* browser_context_impl =
        static_cast<BrowserContextImpl*>(browser_context);
    bool is_safe_browsing_enabled = safe_browsing::IsSafeBrowsingEnabled(
        *browser_context_impl->pref_service());

    if (is_safe_browsing_enabled) {
      bool is_url_real_time_lookup_enabled =
          safe_browsing::RealTimePolicyEngine::CanPerformFullURLLookup(
              browser_context_impl->pref_service(),
              browser_context_impl->IsOffTheRecord(),
              FeatureListCreator::GetInstance()->variations_service());

      // |url_lookup_service| is used when real time url check is enabled.
      safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service =
          is_url_real_time_lookup_enabled
              ? RealTimeUrlLookupServiceFactory::GetForBrowserContext(
                    browser_context)
              : nullptr;
      return GetSafeBrowsingService()->CreateURLLoaderThrottle(
          wc_getter, frame_tree_node_id, url_lookup_service);
    }
#endif
  }
  return nullptr;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ContentBrowserClientImpl::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  if (auto safe_browsing_throttle = MaybeCreateSafeBrowsingURLLoaderThrottle(
          browser_context, wc_getter, frame_tree_node_id);
      safe_browsing_throttle) {
    result.push_back(std::move(safe_browsing_throttle));
  }

  auto signin_throttle =
      SigninURLLoaderThrottle::Create(browser_context, wc_getter);
  if (signin_throttle)
    result.push_back(std::move(signin_throttle));

  // Create prerender URL throttle.
  auto* web_contents = wc_getter.Run();
  auto* no_state_prefetch_contents =
      NoStatePrefetchContentsFromWebContents(web_contents);
  if (no_state_prefetch_contents) {
    result.push_back(std::make_unique<prerender::PrerenderURLLoaderThrottle>(
        prerender::PrerenderHistograms::GetHistogramPrefix(
            no_state_prefetch_contents->origin()),
        GetPrerenderCanceler(web_contents)));
  }

  return result;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ContentBrowserClientImpl::CreateURLLoaderThrottlesForKeepAlive(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  if (auto safe_browsing_throttle = MaybeCreateSafeBrowsingURLLoaderThrottle(
          browser_context, wc_getter, frame_tree_node_id);
      safe_browsing_throttle) {
    result.push_back(std::move(safe_browsing_throttle));
  }

  return result;
}

bool ContentBrowserClientImpl::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // WebLayer handles error cases.
    return true;
  }

  std::string scheme = url.scheme();

  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kWsScheme,
    url::kWssScheme,
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kFileScheme,
    content::kChromeDevToolsScheme,
    content::kChromeUIScheme,
    content::kChromeUIUntrustedScheme,
    url::kDataScheme,
#if BUILDFLAG(IS_ANDROID)
    url::kContentScheme,
#endif  // BUILDFLAG(IS_ANDROID)
    url::kAboutScheme,
    url::kBlobScheme,
    url::kFileSystemScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (scheme == supported_protocol)
      return true;
  }

  return false;
}

std::vector<url::Origin>
ContentBrowserClientImpl::GetOriginsRequiringDedicatedProcess() {
  return site_isolation::GetBrowserSpecificBuiltInIsolatedOrigins();
}

bool ContentBrowserClientImpl::MayReuseHost(
    content::RenderProcessHost* process_host) {
  // If there is currently a no-state prefetcher in progress for the host
  // provided, it may not be shared. We require prefetchers to be by themselves
  // in a separate process so that we can monitor their resource usage.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          process_host->GetBrowserContext());
  if (no_state_prefetch_manager &&
      !no_state_prefetch_manager->MayReuseProcessHost(process_host)) {
    return false;
  }

  return true;
}

void ContentBrowserClientImpl::OverridePageVisibilityState(
    content::RenderFrameHost* render_frame_host,
    content::PageVisibilityState* visibility_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (no_state_prefetch_manager &&
      no_state_prefetch_manager->IsWebContentsPrefetching(web_contents)) {
    *visibility_state = content::PageVisibilityState::kHiddenButPainting;
  }
}

bool ContentBrowserClientImpl::ShouldDisableSiteIsolation(
    content::SiteIsolationMode site_isolation_mode) {
  return site_isolation::SiteIsolationPolicy::
      ShouldDisableSiteIsolationDueToMemoryThreshold(site_isolation_mode);
}

std::vector<std::string>
ContentBrowserClientImpl::GetAdditionalSiteIsolationModes() {
  if (site_isolation::SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled())
    return {"Isolate Password Sites"};
  return {};
}

void ContentBrowserClientImpl::PersistIsolatedOrigin(
    content::BrowserContext* context,
    const url::Origin& origin,
    content::ChildProcessSecurityPolicy::IsolatedOriginSource source) {
  site_isolation::SiteIsolationPolicy::PersistIsolatedOrigin(context, origin,
                                                             source);
}

base::OnceClosure ContentBrowserClientImpl::SelectClientCertificate(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
#if BUILDFLAG(IS_ANDROID)
  if (web_contents) {
    return browser_ui::ShowSSLClientCertificateSelector(
        web_contents, cert_request_info, std::move(delegate));
  }
  // Otherwise, fall through to continuing without a certificate.
#endif
  delegate->ContinueWithCertificate(nullptr, nullptr);
  return base::OnceClosure();
}

bool ContentBrowserClientImpl::CanCreateWindow(
    content::RenderFrameHost* opener,
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
    bool* no_javascript_access) {
  *no_javascript_access = false;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);

  // Block popups if there is no NewTabDelegate.
  TabImpl* tab = TabImpl::FromWebContents(web_contents);
  if (!tab || !tab->has_new_tab_delegate())
    return false;

  if (container_type == content::mojom::WindowContainerType::BACKGROUND ||
      container_type == content::mojom::WindowContainerType::PERSISTENT) {
    // WebLayer does not support extensions/apps, which are the only permitted
    // users of background windows.
    return false;
  }

  // WindowOpenDisposition has a *ton* of types, but the following are really
  // the only ones that should be hit for this code path.
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_POPUP:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_WINDOW:
      break;
    default:
      return false;
  }

  GURL popup_url(target_url);
  opener->GetProcess()->FilterURL(false, &popup_url);
  // Use ui::PAGE_TRANSITION_LINK to match the similar logic in //chrome.
  content::OpenURLParams params(popup_url, referrer, disposition,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated*/ true);
  params.user_gesture = user_gesture;
  params.initiator_origin = source_origin;
  params.source_render_frame_id = opener->GetRoutingID();
  params.source_render_process_id = opener->GetProcess()->GetID();
  params.source_site_instance = opener->GetSiteInstance();
  // The content::OpenURLParams are created just for the delegate, and do not
  // correspond to actual params created by //content, so pass null for the
  // |open_url_params| argument here.
  return blocked_content::MaybeBlockPopup(
             web_contents, &opener_top_level_frame_url,
             std::make_unique<PopupNavigationDelegateImpl>(params, web_contents,
                                                           opener),
             /*open_url_params*/ nullptr, features,
             HostContentSettingsMapFactory::GetForBrowserContext(
                 web_contents->GetBrowserContext())) != nullptr;
}

content::ControllerPresentationServiceDelegate*
ContentBrowserClientImpl::GetControllerPresentationServiceDelegate(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  if (WebLayerFactoryImplAndroid::GetClientMajorVersion() < 88)
    return nullptr;

  if (MediaRouterFactory::IsFeatureEnabled()) {
    MediaRouterFactory::DoPlatformInitIfNeeded();
    return media_router::PresentationServiceDelegateImpl::
        GetOrCreateForWebContents(web_contents);
  }
#endif

  return nullptr;
}

void ContentBrowserClientImpl::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  std::move(callback).Run(
      ProfileImpl::FromBrowserContext(site_instance->GetBrowserContext())
          ->OpenUrl(params));
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
ContentBrowserClientImpl::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;

  TabImpl* tab = TabImpl::FromWebContents(handle->GetWebContents());
  NavigationControllerImpl* navigation_controller = nullptr;
  if (tab) {
    navigation_controller =
        static_cast<NavigationControllerImpl*>(tab->GetNavigationController());
  }

  NavigationImpl* navigation_impl = nullptr;
  if (navigation_controller) {
    navigation_impl =
        navigation_controller->GetNavigationImplFromHandle(handle);
  }

  if (handle->IsInMainFrame()) {
    NavigationUIDataImpl* navigation_ui_data =
        static_cast<NavigationUIDataImpl*>(handle->GetNavigationUIData());

    if ((!navigation_ui_data ||
         !navigation_ui_data->disable_network_error_auto_reload()) &&
        (!navigation_impl ||
         !navigation_impl->disable_network_error_auto_reload()) &&
        IsNetworkErrorAutoReloadEnabled()) {
      auto auto_reload_throttle =
          error_page::NetErrorAutoReloader::MaybeCreateThrottleFor(handle);
      if (auto_reload_throttle)
        throttles.push_back(std::move(auto_reload_throttle));
    }

    // MetricsNavigationThrottle requires that it runs before
    // NavigationThrottles that may delay or cancel navigations, so only
    // NavigationThrottles that don't delay or cancel navigations (e.g.
    // throttles that are only observing callbacks without affecting navigation
    // behavior) should be added before MetricsNavigationThrottle.
    throttles.push_back(
        page_load_metrics::MetricsNavigationThrottle::Create(handle));
    if (TabImpl::FromWebContents(handle->GetWebContents())) {
      throttles.push_back(
          std::make_unique<NavigationErrorNavigationThrottle>(handle));
    }
  }

  // The next highest priority throttle *must* be this as it's responsible for
  // calling to NavigationController for certain events.
  if (tab) {
    auto throttle = navigation_controller->CreateNavigationThrottle(handle);
    if (throttle)
      throttles.push_back(std::move(throttle));
  }

  throttles.push_back(std::make_unique<SSLErrorNavigationThrottle>(
      handle, std::make_unique<SSLCertReporterImpl>(),
      base::BindOnce(&HandleSSLErrorWrapper), base::BindOnce(&IsInHostedApp),
      base::BindOnce(
          &ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps)));

  std::unique_ptr<security_interstitials::InsecureFormNavigationThrottle>
      insecure_form_throttle = security_interstitials::
          InsecureFormNavigationThrottle::MaybeCreateNavigationThrottle(
              handle, std::make_unique<WebLayerSecurityBlockingPageFactory>(),
              nullptr);
  if (insecure_form_throttle) {
    throttles.push_back(std::move(insecure_form_throttle));
  }

  if (auto* throttle_manager =
          subresource_filter::ContentSubresourceFilterThrottleManager::
              FromNavigationHandle(*handle)) {
    throttle_manager->MaybeAppendNavigationThrottles(handle, &throttles);
  }

#if BUILDFLAG(IS_ANDROID)
  if (IsSafebrowsingSupported()) {
    std::unique_ptr<content::NavigationThrottle> safe_browsing_throttle =
        GetSafeBrowsingService()->MaybeCreateSafeBrowsingNavigationThrottleFor(
            handle);
    if (safe_browsing_throttle)
      throttles.push_back(std::move(safe_browsing_throttle));
  }

  if (!navigation_impl || !navigation_impl->disable_intent_processing()) {
    std::unique_ptr<content::NavigationThrottle> intercept_navigation_throttle =
        navigation_interception::InterceptNavigationDelegate::
            MaybeCreateThrottleFor(
                handle, navigation_interception::SynchronyMode::kAsync);
    if (intercept_navigation_throttle)
      throttles.push_back(std::move(intercept_navigation_throttle));
  }
#endif
  return throttles;
}

content::GeneratedCodeCacheSettings
ContentBrowserClientImpl::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  DCHECK(context);
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return content::GeneratedCodeCacheSettings(
      true, 0, ProfileImpl::GetCachePath(context));
}

void ContentBrowserClientImpl::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  // TODO(https://crbug.com/1265864): Move the registry logic below to a
  // dedicated file to ensure security review coverage.
  // TODO(lingqi): Swap the parameters so that lambda functions are not needed.
  associated_registry.AddInterface<autofill::mojom::AutofillDriver>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<autofill::mojom::AutofillDriver>
                 receiver) {
            autofill::ContentAutofillDriverFactory::BindAutofillDriver(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<autofill::mojom::PasswordManagerDriver>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 autofill::mojom::PasswordManagerDriver> receiver) {
            PasswordManagerDriverFactory::BindPasswordManagerDriver(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<
      content_capture::mojom::ContentCaptureReceiver>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             content_capture::mojom::ContentCaptureReceiver> receiver) {
        content_capture::OnscreenContentProvider::BindContentCaptureReceiver(
            std::move(receiver), render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<page_load_metrics::mojom::PageLoadMetrics>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 page_load_metrics::mojom::PageLoadMetrics> receiver) {
            page_load_metrics::MetricsWebContentsObserver::BindPageLoadMetrics(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<
      security_interstitials::mojom::InterstitialCommands>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             security_interstitials::mojom::InterstitialCommands> receiver) {
        security_interstitials::SecurityInterstitialTabHelper::
            BindInterstitialCommands(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<
      subresource_filter::mojom::SubresourceFilterHost>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             subresource_filter::mojom::SubresourceFilterHost> receiver) {
        subresource_filter::ContentSubresourceFilterThrottleManager::
            BindReceiver(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
}

void ContentBrowserClientImpl::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->CreateProcessNodeAndExposeInterfacesToRendererProcess(
          registry, render_process_host);
#if BUILDFLAG(IS_ANDROID)
  auto create_spellcheck_host =
      [](mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
        mojo::MakeSelfOwnedReceiver(std::make_unique<SpellCheckHostImpl>(),
                                    std::move(receiver));
      };
  registry->AddInterface<spellcheck::mojom::SpellCheckHost>(
      base::BindRepeating(create_spellcheck_host),
      content::GetUIThreadTaskRunner({}));

  if (base::FeatureList::IsEnabled(features::kWebLayerSafeBrowsing) &&
      IsSafebrowsingSupported()) {
    GetSafeBrowsingService()->AddInterface(registry, render_process_host);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ContentBrowserClientImpl::BindMediaServiceReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(IS_ANDROID)
  if (auto r = receiver.As<media::mojom::MediaDrmStorage>()) {
    CreateMediaDrmStorage(render_frame_host, std::move(r));
    return;
  }
#endif
}

void ContentBrowserClientImpl::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateWebLayerFrameBinders(render_frame_host, map);
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->ExposeInterfacesToRenderFrame(map);
}

void ContentBrowserClientImpl::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  PageSpecificContentSettingsDelegate::InitializeRenderer(host);
}

void ContentBrowserClientImpl::CreateFeatureListAndFieldTrials() {
  local_state_ = CreateLocalState();
  feature_list_creator_ =
      std::make_unique<FeatureListCreator>(local_state_.get());
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(GetUserAgent());
  feature_list_creator_->SetSystemNetworkContextManager(
      SystemNetworkContextManager::GetInstance());
  feature_list_creator_->CreateFeatureListAndFieldTrials();
}

#if BUILDFLAG(IS_ANDROID)
SafeBrowsingService* ContentBrowserClientImpl::GetSafeBrowsingService() {
  return BrowserProcess::GetInstance()->GetSafeBrowsingService();
}
#endif

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_ANDROID)
void ContentBrowserClientImpl::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if BUILDFLAG(IS_ANDROID)
  base::MemoryMappedFile::Region region;
  int fd = ui::GetMainAndroidPackFd(&region);
  mappings->ShareWithRegion(kWebLayerMainPakDescriptor, fd, region);

  fd = ui::GetCommonResourcesPackFd(&region);
  mappings->ShareWithRegion(kWebLayer100PercentPakDescriptor, fd, region);

  fd = ui::GetLocalePackFd(&region);
  mappings->ShareWithRegion(kWebLayerLocalePakDescriptor, fd, region);

  if (base::android::BundleUtils::IsBundle()) {
    fd = ui::GetSecondaryLocalePackFd(&region);
    mappings->ShareWithRegion(kWebLayerSecondaryLocalePakDescriptor, fd,
                              region);
  } else {
    mappings->ShareWithRegion(kWebLayerSecondaryLocalePakDescriptor,
                              base::GlobalDescriptors::GetInstance()->Get(
                                  kWebLayerSecondaryLocalePakDescriptor),
                              base::GlobalDescriptors::GetInstance()->GetRegion(
                                  kWebLayerSecondaryLocalePakDescriptor));
  }

  int crash_signal_fd =
      crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
  if (crash_signal_fd >= 0)
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
#endif  // BUILDFLAG(IS_ANDROID)
}
#endif

void ContentBrowserClientImpl::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  const base::CommandLine& browser_command_line(
      *base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(::switches::kProcessType);
  if (process_type == ::switches::kRendererProcess) {
    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
        embedder_support::kOriginTrialDisabledFeatures,
        embedder_support::kOriginTrialPublicKey,
    };
    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames);
  }
}

// static
std::unique_ptr<PrefService> ContentBrowserClientImpl::CreateLocalState() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  RegisterPrefs(pref_registry.get());
  base::FilePath path;
  CHECK(base::PathService::Get(DIR_USER_DATA, &path));
  path = path.AppendASCII("Local State");
  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  {
    // Creating the prefs service may require reading the preferences from
    // disk.
    base::ScopedAllowBlocking allow_io;
    return pref_service_factory.Create(pref_registry);
  }
}

#if BUILDFLAG(IS_ANDROID)
bool ContentBrowserClientImpl::WillCreateURLLoaderFactory(
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
    network::mojom::URLLoaderFactoryOverridePtr* factory_override,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  // The navigation API intercepting API only supports main frame navigations.
  if (type != URLLoaderFactoryType::kNavigation || frame->GetParent())
    return false;

  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  TabImpl* tab = TabImpl::FromWebContents(web_contents);
  if (!tab)
    return false;

  auto* navigation_controller =
      static_cast<NavigationControllerImpl*>(tab->GetNavigationController());
  auto* navigation_impl =
      navigation_controller->GetNavigationImplFromId(*navigation_id);
  if (!navigation_impl)
    return false;

  auto response = navigation_impl->TakeResponse();
  if (!response && !ProxyingURLLoaderFactoryImpl::HasCachedInputStream(
                       frame->GetFrameTreeNodeId(),
                       navigation_impl->navigation_entry_unique_id())) {
    return false;
  }

  mojo::PendingReceiver<network::mojom::URLLoaderFactory> proxied_receiver =
      std::move(*factory_receiver);
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
  *factory_receiver = target_factory_remote.InitWithNewPipeAndPassReceiver();

  // Owns itself.
  new ProxyingURLLoaderFactoryImpl(
      std::move(proxied_receiver), std::move(target_factory_remote),
      navigation_impl->GetURL(), std::move(response),
      frame->GetFrameTreeNodeId(),
      navigation_impl->navigation_entry_unique_id());

  return true;
}

content::ContentBrowserClient::WideColorGamutHeuristic
ContentBrowserClientImpl::GetWideColorGamutHeuristic() {
  // Always match window since a mismatch can cause inefficiency in surface
  // flinger.
  return WideColorGamutHeuristic::kUseWindow;
}

std::unique_ptr<content::LoginDelegate>
ContentBrowserClientImpl::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<HttpAuthHandlerImpl>(
      auth_info, web_contents, first_auth_attempt,
      std::move(auth_required_callback));
}

std::unique_ptr<content::TtsEnvironmentAndroid>
ContentBrowserClientImpl::CreateTtsEnvironmentAndroid() {
  return std::make_unique<TtsEnvironmentAndroidImpl>();
}

bool ContentBrowserClientImpl::
    ShouldObserveContainerViewLocationForDialogOverlays() {
  // Observe location changes of the container view as WebLayer might be
  // embedded in a scrollable container and we need to update the position of
  // any DialogOverlays.
  return true;
}

content::BluetoothDelegate* ContentBrowserClientImpl::GetBluetoothDelegate() {
  if (!bluetooth_delegate_) {
    bluetooth_delegate_ = std::make_unique<permissions::BluetoothDelegateImpl>(
        std::make_unique<WebLayerBluetoothDelegateImplClient>());
  }
  return bluetooth_delegate_.get();
}

#endif  // BUILDFLAG(IS_ANDROID)

content::SpeechRecognitionManagerDelegate*
ContentBrowserClientImpl::CreateSpeechRecognitionManagerDelegate() {
  return new WebLayerSpeechRecognitionManagerDelegate();
}

bool ContentBrowserClientImpl::ShouldSandboxNetworkService() {
#if BUILDFLAG(IS_WIN)
  // Weblayer ConfigureNetworkContextParams does not support data migration
  // required for network sandbox to be enabled on Windows.
  return false;
#else
  return ContentBrowserClient::ShouldSandboxNetworkService();
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(ENABLE_ARCORE)
content::XrIntegrationClient*
ContentBrowserClientImpl::GetXrIntegrationClient() {
  if (!XrIntegrationClientImpl::IsEnabled())
    return nullptr;

  if (!xr_integration_client_)
    xr_integration_client_ = std::make_unique<XrIntegrationClientImpl>();
  return xr_integration_client_.get();
}
#endif  // BUILDFLAG(ENABLE_ARCORE)

ukm::UkmService* ContentBrowserClientImpl::GetUkmService() {
#if BUILDFLAG(IS_ANDROID)
  return WebLayerMetricsServiceClient::GetInstance()->GetUkmService();
#else
  return nullptr;
#endif
}

bool ContentBrowserClientImpl::HasErrorPage(int http_status_code) {
  // Use an internal error page, if we have one for the status code.
  return error_page::LocalizedError::HasStrings(
      error_page::Error::kHttpErrorDomain, http_status_code);
}

bool ContentBrowserClientImpl::IsClipboardPasteAllowed(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  content::BrowserContext* browser_context =
      render_frame_host->GetBrowserContext();
  DCHECK(browser_context);

  content::PermissionController* permission_controller =
      browser_context->GetPermissionController();
  blink::mojom::PermissionStatus status =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::CLIPBOARD_READ_WRITE, render_frame_host);

  if (!render_frame_host->HasTransientUserActivation() &&
      status != blink::mojom::PermissionStatus::GRANTED) {
    // Paste requires either user activation, or granted web permission.
    return false;
  }

  return true;
}

bool ContentBrowserClientImpl::ShouldPreconnectNavigation(
    content::BrowserContext* browser_context) {
  return true;
}

}  // namespace weblayer

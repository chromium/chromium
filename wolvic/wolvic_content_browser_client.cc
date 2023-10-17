// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_content_browser_client.h"

#include "base/path_service.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/user_level_memory_pressure_signal_features.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "wolvic/browser/session_settings.h"
#include "wolvic/browser/youtube/youtube_url_loader_request_interceptor.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_main_delegate.h"
#include "wolvic/wolvic_main_parts.h"

namespace content {

namespace {

WolvicContentBrowserClient* g_instance = nullptr;

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
    LOG(ERROR) << __func__ << ": Unique origin.";
    return;
  }

  auto* wolvic_browser_context = static_cast<WolvicBrowserContext*>(
      render_frame_host->GetBrowserContext());
  CHECK(wolvic_browser_context) << "WolvicBrowserContext not available.";

  PrefService* pref_service = wolvic_browser_context->GetPrefService();
  CHECK(pref_service);

  // The object will be deleted on connection error, or when the frame navigates
  // away.
  new cdm::MediaDrmStorageImpl(
      *render_frame_host, pref_service, base::BindRepeating(&CreateOriginId),
      base::BindRepeating(&AllowEmptyOriginIdCB), std::move(receiver));
}

}  // namespace

WolvicContentBrowserClient::WolvicContentBrowserClient()
    : browser_main_parts_(nullptr) {
  DCHECK(!g_instance);
  g_instance = this;
}

WolvicContentBrowserClient::~WolvicContentBrowserClient() {
  g_instance = nullptr;
}

// static
WolvicContentBrowserClient* WolvicContentBrowserClient::Get() {
  return g_instance;
}

content::BrowserContext* WolvicContentBrowserClient::browser_context() {
  return browser_main_parts_->browser_context();
}

content::BrowserContext*
WolvicContentBrowserClient::off_the_record_browser_context() {
  return browser_main_parts_->off_the_record_browser_context();
}

std::unique_ptr<BrowserMainParts>
WolvicContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  CHECK(!browser_main_parts_);
  browser_main_parts_ = new WolvicMainParts();
  return std::unique_ptr<BrowserMainParts>(browser_main_parts_);
}

std::unique_ptr<content::DevToolsManagerDelegate>
WolvicContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<content::ShellDevToolsManagerDelegate>(
      browser_context());
}

#if BUILDFLAG(ENABLE_VR)
XrIntegrationClient* WolvicContentBrowserClient::GetXrIntegrationClient() {
  if (!xr_integration_client_)
    xr_integration_client_ =
        std::make_unique<wolvic::WolvicXrIntegrationClient>(
            base::PassKey<WolvicContentBrowserClient>());
  return xr_integration_client_.get();
}
#endif

std::string WolvicContentBrowserClient::GetUserAgent() {
  auto* settings = wolvic::SessionSettings::Get();
  if (auto user_agent_override = settings->GetUserAgentOverride())
    return *user_agent_override;

  return settings->GetDefaultUserAgent(settings->GetUserAgentMode());
}

blink::UserAgentMetadata WolvicContentBrowserClient::GetUserAgentMetadata() {
  typedef wolvic::SessionSettings::UserAgentMode UserAgentMode;

  auto metadata = embedder_support::GetUserAgentMetadata();
  auto user_agent_mode = wolvic::SessionSettings::Get()->GetUserAgentMode();
  switch (user_agent_mode) {
    case UserAgentMode::kMobile:
    case UserAgentMode::kMobileVR:
      metadata.mobile = true;
      break;
    case UserAgentMode::kDesktop:
      metadata.mobile = false;
      break;
  }
  return metadata;
}

void WolvicContentBrowserClient::ConfigureNetworkContextParams(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  base::FilePath user_data_path;
  base::PathService::Get(SHELL_DIR_USER_DATA, &user_data_path);
  network_context_params->http_cache_directory =
      user_data_path.Append(FILE_PATH_LITERAL("Cache"));
}

void WolvicContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  // The following code adds the command line flag to correctly initialize the
  // UserLevelMemoryPressureSignalGenerator feature. It's copied
  // from chrome/browser/chrome_content_browser_client.cc.
#if BUILDFLAG(IS_ANDROID)
  // The browser process only decides to enable or disable
  // UserLevelMemoryPressureSignalGenerator feature. Renderer processes
  // follow the decision. If the browser process enables the feature, renderer
  // processes will provide private memory footprint for the browser process
  // and will generate memory pressure signals when the browser process
  // requests. So the decision will be provided for renderer processes
  // via commandline flag.
  std::ostringstream user_level_memory_pressure_params;
  if (features::IsUserLevelMemoryPressureSignalEnabledOn4GbDevices()) {
    user_level_memory_pressure_params
        << features::InertIntervalFor4GbDevices().InSeconds() << "s,"
        << features::MinUserMemoryPressureIntervalOn4GbDevices().InSeconds()
        << "s";
  } else if (features::IsUserLevelMemoryPressureSignalEnabledOn6GbDevices()) {
    user_level_memory_pressure_params
        << features::InertIntervalFor6GbDevices().InSeconds() << "s,"
        << features::MinUserMemoryPressureIntervalOn6GbDevices().InSeconds()
        << "s";
  }
  if (user_level_memory_pressure_params.tellp() > 0) {
    command_line->AppendSwitchASCII(
        switches::kUserLevelMemoryPressureSignalParams,
        user_level_memory_pressure_params.str());
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
WolvicContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    int64_t navigation_id,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
      interceptors;

  interceptors.push_back(
      std::make_unique<wolvic::YoutubeURLLoaderRequestInterceptor>());

  return interceptors;
}

void WolvicContentBrowserClient::BindMediaServiceReceiver(
    RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<media::mojom::MediaDrmStorage>()) {
    CreateMediaDrmStorage(render_frame_host, std::move(r));
  }
}

}  // namespace content

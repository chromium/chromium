// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_content_browser_client.h"
#include <memory>

#include "base/path_service.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "wolvic/browser/dialogs/http_auth_manager.h"
#include "wolvic/browser/service_tab_launcher.h"
#include "wolvic/browser/session_settings.h"
#include "wolvic/browser/wolvic_browser_interface_binders.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_main_delegate.h"
#include "wolvic/wolvic_main_parts.h"

namespace wolvic {

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

std::unique_ptr<content::BrowserMainParts>
WolvicContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  CHECK(!browser_main_parts_);
  browser_main_parts_ = new WolvicMainParts();
  return std::unique_ptr<content::BrowserMainParts>(browser_main_parts_);
}

std::unique_ptr<content::DevToolsManagerDelegate>
WolvicContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<content::ShellDevToolsManagerDelegate>(
      browser_context());
}

std::unique_ptr<content::LoginDelegate>
WolvicContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<HttpAuthManager>(auth_info, web_contents,
                                           first_auth_attempt,
                                           std::move(auth_required_callback));
}

#if BUILDFLAG(ENABLE_VR)
content::XrIntegrationClient*
WolvicContentBrowserClient::GetXrIntegrationClient() {
  if (!xr_integration_client_)
    xr_integration_client_ = std::make_unique<WolvicXrIntegrationClient>(
        base::PassKey<WolvicContentBrowserClient>());
  return xr_integration_client_.get();
}
#endif

std::string WolvicContentBrowserClient::GetUserAgent() {
  auto* settings = SessionSettings::Get();
  if (auto user_agent_override = settings->GetUserAgentOverride())
    return *user_agent_override;

  return settings->GetDefaultUserAgent(settings->GetUserAgentMode());
}

blink::UserAgentMetadata WolvicContentBrowserClient::GetUserAgentMetadata() {
  typedef SessionSettings::UserAgentMode UserAgentMode;

  auto metadata = embedder_support::GetUserAgentMetadata();
  auto user_agent_mode = SessionSettings::Get()->GetUserAgentMode();
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
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  base::FilePath user_data_path;
  base::PathService::Get(content::SHELL_DIR_USER_DATA, &user_data_path);
  network_context_params->file_paths = network::mojom::NetworkContextFilePaths::New();
  network_context_params->file_paths->http_cache_directory =
      user_data_path.Append(FILE_PATH_LITERAL("Cache"));

  // TODO: Set the desktop user agent by the default, and revisit this to set
  // the setting value if the payment request solves the UA issue.

  // These values will be used when the network requst has the empty http
  // header. All network requests created by renderer(web page) already have
  // the http header, so the value will be used only for the network requests
  // created by the native code like the payment request.
  auto* settings = SessionSettings::Get();
  network_context_params->user_agent =
      settings->GetDefaultUserAgent(SessionSettings::UserAgentMode::kDesktop);
  network_context_params->accept_language = "en-us,en";
}

void WolvicContentBrowserClient::BindMediaServiceReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<media::mojom::MediaDrmStorage>()) {
    CreateMediaDrmStorage(render_frame_host, std::move(r));
  }
}

void WolvicContentBrowserClient::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  content::BrowserContext* browser_context = site_instance->GetBrowserContext();
  // TODO (jfernandez): Explose an alternate approach based on the
  // TabModelJniBridge::HandlePopupNavigation
  ServiceTabLauncher::GetInstance()->LaunchTab(browser_context, params,
                                               std::move(callback));
}

void WolvicContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
    content::RenderFrameHost& render_frame_host,
    blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<
      autofill::mojom::PasswordManagerDriver>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
             receiver) {
        password_manager::ContentPasswordManagerDriverFactory::
            BindPasswordManagerDriver(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
}

void WolvicContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  wolvic::internal::PopulateWolvicFrameBinders(map, render_frame_host);
}

}  // namespace wolvic

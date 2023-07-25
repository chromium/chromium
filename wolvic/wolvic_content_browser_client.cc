// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_content_browser_client.h"

#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "wolvic/browser/session_settings.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_main_delegate.h"
#include "wolvic/wolvic_main_parts.h"

namespace content {

namespace {

WolvicContentBrowserClient* g_instance = nullptr;

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

content::BrowserContext* WolvicContentBrowserClient::GetBrowserContext() {
  return browser_main_parts_->browser_context();
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
      GetBrowserContext());
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

}  // namespace content

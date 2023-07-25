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

// Adds the given platform to the user agent string. For example, if
// we add "Mobile VR" platform to the following user agent
// "Mozilla/5.0 (Linux; Android 8.0.0; Quest 2)", the result will be:
// "Mozilla/5.0 (Linux; Android 8.0.0; Quest 2; Mobile VR)".
void AddPlatformToUserAgent(const std::string& platform,
                            std::string& user_agent) {
  size_t pos = user_agent.find(')');
  if (pos != std::string::npos) {
    user_agent.insert(pos, "; " + platform);
  }
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
  typedef wolvic::SessionSettings::UserAgentMode UserAgentMode;
  auto* session_settings = wolvic::SessionSettings::Get();

  auto user_agent_override = session_settings->GetUserAgentOverride();
  if (user_agent_override)
    return *user_agent_override;

  std::string user_agent = session_settings->GetDefaultUserAgent();
  auto user_agent_mode = session_settings->GetUserAgentMode();
  switch (user_agent_mode) {
    case UserAgentMode::kMobile:
      AddPlatformToUserAgent("Mobile", user_agent);
      break;
    case UserAgentMode::kMobileVR:
      AddPlatformToUserAgent("Mobile VR", user_agent);
      break;
    case UserAgentMode::kDesktop:
      // do nothing
      break;
  }
  return user_agent;
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

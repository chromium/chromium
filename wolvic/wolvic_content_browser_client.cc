// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_content_browser_client.h"

#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "wolvic/wolvic_content_main_delegate.h"
#include "wolvic/wolvic_main_parts.h"

namespace content {

WolvicContentBrowserClient::WolvicContentBrowserClient()
    : browser_main_parts_(nullptr) {}

WolvicContentBrowserClient::~WolvicContentBrowserClient() {}

std::unique_ptr<BrowserMainParts>
WolvicContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  CHECK(!browser_main_parts_);
  browser_main_parts_ = new WolvicMainParts();
  return std::unique_ptr<BrowserMainParts>(browser_main_parts_);
}

WolvicBrowserContext* WolvicContentBrowserClient::browser_context() {
  return browser_main_parts_->browser_context();
}

std::string WolvicContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

std::string WolvicContentBrowserClient::GetUserAgentBasedOnPolicy(
    content::BrowserContext* context) {
  auto* delegate = WolvicContentMainDelegate::Get();

  const PrefService* prefs = delegate->GetPrefs();
  embedder_support::ForceMajorVersionToMinorPosition
      force_major_version_to_minor =
          embedder_support::GetMajorToMinorFromPrefs(prefs);
  embedder_support::UserAgentReductionEnterprisePolicyState
      user_agent_reduction =
          embedder_support::GetUserAgentReductionFromPrefs(prefs);
  switch (user_agent_reduction) {
    case embedder_support::UserAgentReductionEnterprisePolicyState::
        kForceDisabled:
      return embedder_support::GetFullUserAgent(force_major_version_to_minor);
    case embedder_support::UserAgentReductionEnterprisePolicyState::
        kForceEnabled:
      return embedder_support::GetReducedUserAgent(
          force_major_version_to_minor);
    case embedder_support::UserAgentReductionEnterprisePolicyState::kDefault:
    default:
      return embedder_support::GetUserAgent(force_major_version_to_minor,
                                            user_agent_reduction);
  }
}

std::string WolvicContentBrowserClient::GetFullUserAgent() {
  return embedder_support::GetFullUserAgent();
}

std::string WolvicContentBrowserClient::GetReducedUserAgent() {
  return embedder_support::GetReducedUserAgent();
}

}  // namespace content

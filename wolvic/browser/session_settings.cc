// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/session_settings.h"

#include "base/check.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/common/user_agent.h"

namespace wolvic {

namespace {

SessionSettings* g_instance = nullptr;

}  // namespace

SessionSettings::SessionSettings() {
  DCHECK(!g_instance);
  g_instance = this;
}

SessionSettings::~SessionSettings() {
  g_instance = nullptr;
}

SessionSettings* SessionSettings::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void SessionSettings::SetUserAgentMode(UserAgentMode value) {
  user_agent_mode_ = value;
}

SessionSettings::UserAgentMode SessionSettings::GetUserAgentMode() const {
  return user_agent_mode_;
}

void SessionSettings::SetUserAgentOverride(
    const absl::optional<std::string>& value) {
  user_agent_override_ = value;
}

absl::optional<std::string> SessionSettings::GetUserAgentOverride() const {
  return user_agent_override_;
}

std::string SessionSettings::GetDefaultUserAgent(UserAgentMode mode) const {
  static std::string kWolvicUserAgent;
  static std::string kWolvicUserAgentVR;
  static std::string kWolvicUserAgentDesktop;

  static std::once_flag once_flag;
  std::call_once(once_flag, [] {
    kWolvicUserAgent = embedder_support::GetUserAgent() + " Mobile";
    kWolvicUserAgentVR = embedder_support::GetUserAgent() + " Mobile VR";

    const char kLinuxInfoStr[] = "X11; Linux x86_64";
    kWolvicUserAgentDesktop = content::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, embedder_support::GetProductAndVersion());
  });

  switch (mode) {
    case UserAgentMode::kMobile:
      return kWolvicUserAgent;
    case UserAgentMode::kMobileVR:
      return kWolvicUserAgentVR;
    case UserAgentMode::kDesktop:
      return kWolvicUserAgentDesktop;
  }
}

}  // namespace wolvic

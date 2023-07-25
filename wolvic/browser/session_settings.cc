// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/session_settings.h"

#include "base/check.h"
#include "components/embedder_support/user_agent_utils.h"

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

std::string SessionSettings::GetDefaultUserAgent() const {
  return embedder_support::GetUserAgent();
}

}  // namespace wolvic
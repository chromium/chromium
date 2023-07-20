// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_SESSION_SETTINGS_H_
#define WOLVIC_BROWSER_SESSION_SETTINGS_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace wolvic {

// A singleton class holding all settings for the current session.
class SessionSettings {
 public:
  enum class UserAgentMode {
    // values have to be synchronized with SessionSettings.java
    kMobile = 0,
    kDesktop = 1,
    kMobileVR = 2,
  };

  explicit SessionSettings();
  SessionSettings(const SessionSettings&) = delete;
  SessionSettings& operator=(const SessionSettings&) = delete;
  ~SessionSettings();

  // Returns the singleton instance.
  static SessionSettings* Get();

  void SetUserAgentMode(UserAgentMode value);
  UserAgentMode GetUserAgentMode() const;
  void SetUserAgentOverride(const absl::optional<std::string>& value);
  absl::optional<std::string> GetUserAgentOverride() const;

 private:
  UserAgentMode user_agent_mode_ = UserAgentMode::kMobile;
  absl::optional<std::string> user_agent_override_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_SESSION_SETTINGS_H_
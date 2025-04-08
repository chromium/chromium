// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_SESSION_MANAGER_H_
#define UI_OZONE_PUBLIC_PLATFORM_SESSION_MANAGER_H_

#include <optional>
#include <string>

#include "base/component_export.h"

namespace ui {

// PlatformSessionManager must be implemented by platforms which support display
// server side session management, such as, xdg-session-management protocol in
// Ozone/Wayland.
//
// A singleton instance of this class is typically created and owned by the
// ozone platform object. Access and manipulation of it must happen in the UI
// thread of the browser process.
class COMPONENT_EXPORT(OZONE_BASE) PlatformSessionManager {
 public:
  enum class RestoreReason { kLaunch, kPostCrash };

  PlatformSessionManager() = default;
  virtual ~PlatformSessionManager() = default;

  PlatformSessionManager(const PlatformSessionManager&) = delete;
  PlatformSessionManager& operator=(const PlatformSessionManager&) = delete;

  // Before creating platform windows, clients of this interface are expected to
  // either (1) create a new session, or (2) restore a previously created one.
  // The returned "session id" string must be then stored somehow so that it can
  // be used later on in future session restores. If the operation fails for
  // some reason, nullopt is returned.
  virtual std::optional<std::string> CreateSession() = 0;
  virtual std::optional<std::string> RestoreSession(
      const std::string& session_name,
      RestoreReason reason) = 0;

  // Requests the platform session manager to stop tracking state of window
  // identified by `window_id`, for a given session whose id is `session_id`.
  virtual void RemoveWindow(const std::string& session_id,
                            int32_t window_id) = 0;
};

// PlatformSessionWindowData encapsulates the data required by the ozone
// platform to determine how to restore/add a toplevel platform window to a
// given session, identified by `session_id`. If present, `restored_id` is used
// to tentatively restore the associated window, whose id, once fully
// configured, will be updated to `window_id`.
struct COMPONENT_EXPORT(OZONE_BASE) PlatformSessionWindowData {
  std::string session_id;
  int32_t window_id = 0;
  std::optional<int32_t> restore_id;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_SESSION_MANAGER_H_

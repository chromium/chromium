// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/xdg_session.h"
#include "ui/ozone/public/platform_session_manager.h"

namespace ui {

class XdgSession;

class XdgSessionManager : public wl::GlobalObjectRegistrar<XdgSessionManager>,
                          public PlatformSessionManager {
 public:
  static constexpr const char kInterfaceName[] = "xx_session_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  XdgSessionManager(wl::Object<xx_session_manager_v1> manager,
                    WaylandConnection* connection);
  XdgSessionManager(const XdgSessionManager&) = delete;
  XdgSessionManager& operator=(const XdgSessionManager&) = delete;
  ~XdgSessionManager() override;

  // PlatformSessionManager:
  std::optional<std::string> CreateSession() override;
  std::optional<std::string> RestoreSession(const std::string& session_id,
                                            RestoreReason reason) override;
  void RemoveWindow(const std::string& session_id, int32_t window_id) override;

  XdgSession* GetSession(const std::string& session_id) const;

 private:
  // XdgSession requests to destroy itself using this function when handling
  // xdg_session.replaced event.
  friend class XdgSession;
  void DestroySession(XdgSession* session);

  // If needed, waits for either `created` or `recovered` from the server
  // to make sure it can proceed with further add or recover toplevel
  // operations.
  void EnsureReady(const XdgSession& session);

  const raw_ptr<WaylandConnection> connection_;

  wl::Object<xx_session_manager_v1> manager_;

  std::vector<std::unique_ptr<XdgSession>> sessions_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_MANAGER_H_

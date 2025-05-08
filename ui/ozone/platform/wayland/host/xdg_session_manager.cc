// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_session_manager.h"

#include <xx-session-management-v1-client-protocol.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/xdg_session.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel.h"
#include "ui/ozone/public/platform_session_manager.h"

namespace ui {

namespace {

constexpr uint32_t kMaxVersion = 1;

}  // namespace

// static
void XdgSessionManager::Instantiate(WaylandConnection* connection,
                                    wl_registry* registry,
                                    uint32_t name,
                                    const std::string& interface,
                                    uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->session_manager_) {
    return;
  }

  auto instance = wl::Bind<::xx_session_manager_v1>(
      registry, name, std::min(version, kMaxVersion));
  if (!instance) {
    LOG(ERROR) << "Failed to bind " << kInterfaceName;
    return;
  }

  connection->session_manager_ =
      std::make_unique<XdgSessionManager>(std::move(instance), connection);
}

// XdgSessionManager:

XdgSessionManager::XdgSessionManager(wl::Object<xx_session_manager_v1> manager,
                                     WaylandConnection* connection)
    : connection_(connection), manager_(std::move(manager)) {}

XdgSessionManager::~XdgSessionManager() = default;

std::optional<std::string> XdgSessionManager::CreateSession() {
  auto session = std::make_unique<XdgSession>(
      xx_session_manager_v1_get_session(
          manager_.get(), XX_SESSION_MANAGER_V1_REASON_LAUNCH, nullptr),
      this);
  EnsureReady(*session);

  if (!session) {
    PLOG(ERROR) << "Failed to create a XDG Session";
    return std::nullopt;
  }
  EnsureReady(*session);
  auto result = session->state() == XdgSession::State::kCreated
                    ? std::optional<std::string>(session->id())
                    : std::nullopt;
  sessions_.push_back(std::move(session));
  return result;
}

std::optional<std::string> XdgSessionManager::RestoreSession(
    const std::string& session_id,
    RestoreReason reason) {
  CHECK(!session_id.empty());
  if (auto* existing_session = GetSession(session_id)) {
    return existing_session->id();
  }

  const uint32_t wl_reason =
      reason == PlatformSessionManager::RestoreReason::kPostCrash
          ? XX_SESSION_MANAGER_V1_REASON_RECOVER
          : XX_SESSION_MANAGER_V1_REASON_LAUNCH;
  auto session = std::make_unique<XdgSession>(
      xx_session_manager_v1_get_session(manager_.get(), wl_reason,
                                        session_id.c_str()),
      this, session_id);
  EnsureReady(*session);

  // If, for some reason, the provided compositor fails to retrieve the session
  // data corresponding to `session_id`, it will treat `session_id` as null
  // and will tentatively create a new session, thus the condition below.
  auto result = (session->state() == XdgSession::State::kRestored ||
                 session->state() == XdgSession::State::kCreated)
                    ? std::optional<std::string>(session->id())
                    : std::nullopt;
  sessions_.push_back(std::move(session));
  return result;
}

void XdgSessionManager::RemoveWindow(const std::string& session_id,
                                     const int32_t window_id) {
  if (auto* session = GetSession(session_id)) {
    session->RemoveToplevel(window_id);
    return;
  }
  DLOG(WARNING) << "No session found for id=" << session_id;
}

XdgSession* XdgSessionManager::GetSession(const std::string& session_id) const {
  auto it = std::ranges::find(sessions_, session_id, &XdgSession::id);
  return it != sessions_.end() ? it->get() : nullptr;
}

void XdgSessionManager::DestroySession(XdgSession* session) {
  auto [begin, end] = std::ranges::remove(sessions_, session,
                                          &std::unique_ptr<XdgSession>::get);
  sessions_.erase(begin, end);
}

void XdgSessionManager::EnsureReady(const XdgSession& session) {
  CHECK_NE(session.state(), XdgSession::State::kInert);
  // TODO(crbug.com/352081012): Consider implementing a timeout mechanism.
  while (session.state() == XdgSession::State::kPending) {
    connection_->RoundTripQueue();
  }
}

}  // namespace ui

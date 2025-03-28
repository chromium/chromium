// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_session.h"

#include <xx-session-management-v1-client-protocol.h>

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/xdg_session_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

namespace ui {

XdgSession::XdgSession(struct xx_session_v1* session,
                       XdgSessionManager* manager,
                       const std::string& requested_id)
    : session_(session), id_(requested_id), manager_(CHECK_DEREF(manager)) {
  static constexpr struct xx_session_v1_listener kSessionListener = {
      .created = OnCreated,
      .restored = OnRestored,
      .replaced = OnReplaced,
  };
  CHECK(session_);
  xx_session_v1_add_listener(session_.get(), &kSessionListener, this);
}

XdgSession::~XdgSession() {
  observers_.Notify(&Observer::OnSessionDestroying);
}

std::unique_ptr<XdgToplevelSession> XdgSession::TrackToplevel(
    WaylandToplevelWindow* toplevel,
    int32_t toplevel_id,
    Action action) {
  if (state_ == State::kInert) {
    return nullptr;
  }

  CHECK(toplevel->shell_toplevel());
  auto* xdg_toplevel =
      toplevel->shell_toplevel()->AsXDGToplevelWrapper()->xdg_toplevel_.get();

  CHECK_NE(state_, State::kPending);
  auto id = base::NumberToString(toplevel_id);
  auto* request = action == Action::kRestore ? xx_session_v1_restore_toplevel
                                             : xx_session_v1_add_toplevel;
  wl::Object<xx_toplevel_session_v1> toplevel_session;

  toplevel_session.reset(request(session_.get(), xdg_toplevel, id.c_str()));
  return std::make_unique<XdgToplevelSession>(std::move(toplevel_session),
                                              action);
}

void XdgSession::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void XdgSession::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void XdgSession::OnCreated(void* data,
                           struct xx_session_v1* xx_session_v1,
                           const char* id) {
  auto* self = static_cast<XdgSession*>(data);
  CHECK_EQ(self->state_, State::kPending);
  self->state_ = State::kCreated;
  self->id_ = std::string(id);
  DVLOG(1) << "New session created for session_id=" << self->id_;
}

// static
void XdgSession::OnRestored(void* data, struct xx_session_v1* xx_session_v1) {
  auto* self = static_cast<XdgSession*>(data);
  CHECK_EQ(self->state_, State::kPending);
  self->state_ = State::kRestored;
  DVLOG(1) << "Restored session with session_id=" << self->id_;
}

// static
void XdgSession::OnReplaced(void* data, struct xx_session_v1* xx_session_v1) {
  auto* self = static_cast<XdgSession*>(data);
  DVLOG(1) << "Replaced received for session_id=" << self->id_;
  // Sessions are owned by a session manager, thus ask it to be destroyed.
  // `observers_` are then notified about it (see XdgSession's dtor) and are
  // responsible for clearing any related state.
  self->state_ = State::kInert;
  self->manager_->DestroySession(self);
}

XdgToplevelSession::XdgToplevelSession(
    wl::Object<xx_toplevel_session_v1> session,
    XdgSession::Action action)
    : toplevel_session_(std::move(session)), action_(action) {}

XdgToplevelSession::~XdgToplevelSession() = default;

void XdgToplevelSession::Remove() {
  if (!toplevel_session_) {
    return;
  }
  // xx_toplevel_session_v1.remove also "deletes" the proxy object, so `release`
  // must be used in this case to hand over `toplevel_session_` and avoid
  // segfault at XdgToplevelSession destruction time.
  xx_toplevel_session_v1_remove(toplevel_session_.release());
}

}  // namespace ui

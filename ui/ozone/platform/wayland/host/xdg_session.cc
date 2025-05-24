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
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/xdg_session_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

// The experimental version of session-management protocol, shipped with Mutter
// 47/48, does not include `xdg_session.remove_toplvel(id)` request, which makes
// it harder to remove unmapped toplevels from the session. To work around it, a
// dummy window is initialized up to the point where the corresponding
// toplevel-session is recovered and then removed from the session, because of
// which the dummy delegate below is needed.
//
// TODO(crbug.com/409099413): Remove when support for the experimental session
// management protocol support gets dropped.
class DummyDelegate final : public PlatformWindowDelegate {
 public:
  static DummyDelegate* Get() {
    static base::NoDestructor<DummyDelegate> instance;
    return instance.get();
  }

  DummyDelegate() = default;
  DummyDelegate(const DummyDelegate&) = delete;
  DummyDelegate& operator=(const DummyDelegate&) = delete;
  ~DummyDelegate() override = default;

  // PlatformWindowDelegate:
  void OnBoundsChanged(const BoundsChange&) override {}
  void OnDamageRect(const gfx::Rect&) override {}
  void DispatchEvent(Event*) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(PlatformWindowState, PlatformWindowState) override {
  }
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget) override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
  int64_t OnStateUpdate(const State&, const State&) override { return -1; }
};

}  // namespace

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

  CHECK(toplevel->xdg_toplevel());
  auto* xdg_toplevel = toplevel->xdg_toplevel()->wl_object();

  CHECK_NE(state_, State::kPending);
  auto id = base::NumberToString(toplevel_id);
  auto* request = action == Action::kRestore ? xx_session_v1_restore_toplevel
                                             : xx_session_v1_add_toplevel;
  wl::Object<xx_toplevel_session_v1> toplevel_session;

  toplevel_session.reset(request(session_.get(), xdg_toplevel, id.c_str()));
  return std::make_unique<XdgToplevelSession>(std::move(toplevel_session),
                                              action);
}

void XdgSession::RemoveToplevel(int32_t toplevel_id) {
  CHECK_NE(state_, State::kPending);
  WaylandConnection* const connection = manager_->connection_.get();
  auto windows = connection->window_manager()->GetAllWindows();
  auto found_it = std::ranges::find_if(
      windows, [&id = id_, &toplevel_id](WaylandWindow* window) {
        auto* toplevel = window->AsWaylandToplevelWindow();
        return toplevel && toplevel->session_id() == id &&
               toplevel->session_toplevel_id() == toplevel_id;
      });
  if (found_it != windows.end()) {
    auto* toplevel = (*found_it)->AsWaylandToplevelWindow();
    std::unique_ptr<XdgToplevelSession> toplevel_session =
        toplevel->TakeToplevelSession();
    if (!toplevel_session) {
      return;
    }
    toplevel_session->Remove();
    return;
  }

  // Chromium usually removes windows from session at browser startup, after
  // retrieving session commands from disk. The experimental version of
  // session-management protocol shipped with Mutter 47/48, though, does not
  // include `xdg_session.remove_toplvel(id)` request, thus to work around it a
  // dummy window is initialized up to the point where the corresponding
  // toplevel-session is recovered and then removed from the session.

  // No-op if already removing `toplevel_id`.
  if (std::ranges::find(pending_removals_, toplevel_id, [](const auto& window) {
        return window->AsWaylandToplevelWindow()->session_toplevel_id();
      }) != pending_removals_.end()) {
    return;
  }

  // Passing in a valid `restore_id` and null `window_id` as init
  // parameters for window creation here means the xdg_toplevel_session
  // will be recovered and thereafter removed.
  PlatformWindowInitProperties init_properties;
  init_properties.session_id = id_;
  init_properties.session_window_restore_id = toplevel_id;

  auto dummy_window = WaylandWindow::Create(DummyDelegate::Get(), connection,
                                            std::move(init_properties));
  if (!dummy_window) {
    DLOG(WARNING) << "Failed to create dummy window to"
                  << " remove it from the xdg-session";
    return;
  }

  DVLOG(1) << "Requesting toplevel removal for toplevel_id=" << toplevel_id
           << " pending_removals=" << pending_removals_.size();
  pending_removals_.push_back(std::move(dummy_window));
  if (!removal_observer_.IsObserving()) {
    removal_observer_.Observe(connection->window_manager());
  }

  // Note: the dummy window won't actually show up, this is only required to
  // instantiate the required protocol objects, ie: xdg_surface, xdg_toplevel,
  // such that it's possible to issue xdg_session.restore_toplevel. Once the
  // first configure sequence is received, this object will be notified via
  // OnWindowRemovedFromSession and the dummy window gets destroyed.
  pending_removals_.back()->Show(/*inactive=*/true);
}

void XdgSession::OnWindowRemovedFromSession(WaylandWindow* window) {
  CHECK(window->AsWaylandToplevelWindow());
  CHECK(removal_observer_.IsObserving());
  DVLOG(1) << "Completing toplevel removal for toplevel_id="
           << window->AsWaylandToplevelWindow()->session_toplevel_id()
           << " pending_removals=" << pending_removals_.size();

  auto [begin, end] = std::ranges::remove(pending_removals_, window,
                                          &std::unique_ptr<WaylandWindow>::get);
  pending_removals_.erase(begin, end);
  if (pending_removals_.empty()) {
    removal_observer_.Reset();
  }
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

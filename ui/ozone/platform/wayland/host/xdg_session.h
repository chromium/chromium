// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

class WaylandToplevelWindow;
class XdgSessionManager;
class XdgToplevelSession;

class XdgSession final : public WaylandWindowObserver {
 public:
  enum class Action {
    kRestore,
    kAdd,
  };

  enum class State {
    kPending,
    kCreated,
    kRestored,
    kInert,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSessionDestroying() = 0;

   protected:
    ~Observer() override = default;
  };

  XdgSession(struct xx_session_v1* session,
             XdgSessionManager* manager,
             const std::string& requested_id = {});
  XdgSession(const XdgSession&) = delete;
  XdgSession& operator=(const XdgSession&) = delete;
  ~XdgSession() final;

  State state() const { return state_; }
  std::string id() const { return id_; }

  std::unique_ptr<XdgToplevelSession> TrackToplevel(
      WaylandToplevelWindow* toplevel,
      int32_t toplevel_id,
      Action action);
  void RemoveToplevel(int32_t toplevel_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // WaylandWindowObserver:
  void OnWindowRemovedFromSession(WaylandWindow* window) final;

  // xx_session_v1_listener callbacks:
  static void OnCreated(void* data,
                        struct xx_session_v1* xx_session_v1,
                        const char* id);
  static void OnRestored(void* data, struct xx_session_v1* xx_session_v1);
  static void OnReplaced(void* data, struct xx_session_v1* xx_session_v1);

  wl::Object<xx_session_v1> session_;
  State state_ = State::kPending;
  std::string id_;

  std::vector<std::unique_ptr<WaylandWindow>> pending_removals_;
  base::ScopedObservation<WaylandWindowManager, WaylandWindowObserver>
      removal_observer_{this};

  // XdgSessionManager instance is guaranteed to outlive sessions and sessions
  // are always owned by a session manager, so it's safe to store this as a
  // reference rather than a pointer.
  const raw_ref<XdgSessionManager> manager_;

  base::ObserverList<Observer> observers_;
};

class XdgToplevelSession {
 public:
  XdgToplevelSession(wl::Object<xx_toplevel_session_v1> session,
                     XdgSession::Action action);
  ~XdgToplevelSession();

  XdgToplevelSession(const XdgToplevelSession&) = delete;
  XdgToplevelSession& operator=(const XdgToplevelSession&) = delete;

  XdgSession::Action action() const { return action_; }

  void Remove();

 private:
  wl::Object<xx_toplevel_session_v1> toplevel_session_;
  const XdgSession::Action action_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_H_

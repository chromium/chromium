// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SESSION_H_

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandToplevelWindow;
class XdgToplevelSession;

class XdgSession {
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

  explicit XdgSession(struct xx_session_v1* session,
                      const std::string& requested_id = {});
  XdgSession(const XdgSession&) = delete;
  XdgSession& operator=(const XdgSession&) = delete;
  ~XdgSession();

  State state() const { return state_; }
  std::string id() const { return id_; }

  std::unique_ptr<XdgToplevelSession> TrackToplevel(
      WaylandToplevelWindow* toplevel,
      int32_t toplevel_id,
      Action action);

  base::WeakPtr<XdgSession> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  // xx_session_v1_listener callbacks:
  static void OnCreated(void* data,
                        struct xx_session_v1* xx_session_v1,
                        const char* id);
  static void OnRestored(void* data, struct xx_session_v1* xx_session_v1);
  static void OnReplaced(void* data, struct xx_session_v1* xx_session_v1);

  wl::Object<xx_session_v1> session_;
  State state_ = State::kPending;
  std::string id_;

  base::WeakPtrFactory<XdgSession> weak_factory_{this};
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

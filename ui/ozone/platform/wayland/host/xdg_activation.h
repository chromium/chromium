// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

// Implements the XDG activation Wayland protocol extension.
class XdgActivation : public wl::GlobalObjectRegistrar<XdgActivation> {
 public:
  static constexpr char kInterfaceName[] = "xdg_activation_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  XdgActivation(wl::Object<xdg_activation_v1> xdg_activation_v1,
                WaylandConnection* connection);
  XdgActivation(const XdgActivation&) = delete;
  XdgActivation& operator=(const XdgActivation&) = delete;
  ~XdgActivation();

  // Requests activation of the `surface`.
  // The actual activation happens asynchronously, after a round trip to the
  // server.
  // If there is another unfinished activation request, the method chains the
  // new request in the `activation_queue_` and handles it after the current
  // request is completed.
  // Does nothing if no other window is currently active.
  void Activate(wl_surface* surface) const;

 private:
  class Token;

  void OnActivateDone(wl_surface* surface, std::string token);

  // Wayland object wrapped by this class.
  wl::Object<xdg_activation_v1> xdg_activation_v1_;
  // The actual activation token.
  mutable std::unique_ptr<Token> token_;
  // Surfaces to activate next.
  mutable base::queue<raw_ptr<wl_surface>> activation_queue_;

  const raw_ptr<WaylandConnection> connection_;

  base::WeakPtrFactory<XdgActivation> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_

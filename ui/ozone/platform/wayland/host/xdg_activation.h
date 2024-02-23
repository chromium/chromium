// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

// Implements the XDG activation Wayland protocol extension.
class XdgActivation : public wl::GlobalObjectRegistrar<XdgActivation> {
 public:
  static constexpr char kInterfaceName[] = "xdg_activation_v1";
  using RequestNewTokenCallback = base::OnceCallback<void(std::string token)>;

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

  // Requests activation of the `surface` using the `token` received from the
  // app that launched us.
  void Activate(wl_surface* surface, const std::string& token) const;

  // Request a new activation token from the compositor for launching an
  // external app.
  // The token is received asynchronously, after a round trip to the server, at
  // which point the provided `callback` is called.
  // If there is another unfinished request, the method chains the new request
  // in the `request_token_queue_` and handles it after the current request is
  // completed.
  // TODO(https://crbug.com/40747285): Make use of this new API when launching
  // external apps.
  void RequestNewToken(RequestNewTokenCallback callback) const;

 private:
  class Token;

  void OnTokenReceived(RequestNewTokenCallback callback, std::string token);

  // Wayland object wrapped by this class.
  wl::Object<xdg_activation_v1> xdg_activation_v1_;
  // The actual activation token.
  mutable std::unique_ptr<Token> token_;
  // Pending token requests.
  mutable base::queue<RequestNewTokenCallback> request_token_queue_;

  const raw_ptr<WaylandConnection> connection_;

  base::WeakPtrFactory<XdgActivation> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/task/sequenced_task_runner.h"
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

  // Requests activation of the `surface` using the `token` received from the
  // app that launched us.
  void Activate(wl_surface* surface, const std::string& token) const;

  // Request a new activation token from the compositor for launching an
  // external app.
  // The token is received asynchronously and the provided `callback` is called
  // after the server responds to the request or if the request times out.
  // If there is an unfinished request, the method chains the new request in the
  // `token_request_queue_` and initiates the next token request to the server
  // after the server responds to the current request or timeout occurs.
  void RequestNewToken(base::nix::XdgActivationTokenCallback callback) const;

 private:
  class TokenRequest;

  void OnTokenRequestCompleted(base::nix::XdgActivationTokenCallback callback,
                               std::string token);

  // Wayland object wrapped by this class.
  wl::Object<xdg_activation_v1> xdg_activation_v1_;
  // Pending token requests.
  mutable base::queue<std::unique_ptr<TokenRequest>> token_request_queue_;

  const raw_ptr<WaylandConnection> connection_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  base::WeakPtrFactory<XdgActivation> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_ACTIVATION_H_

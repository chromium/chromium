// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_activation.h"

#include <xdg-activation-v1-client-protocol.h>

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {
constexpr uint32_t kMaxVersion = 1;
}

// Wraps the actual activation token.
class XdgActivation::TokenRequest {
 public:
  TokenRequest(xdg_activation_v1* xdg_activation,
               WaylandConnection* connection,
               base::nix::XdgActivationTokenCallback callback);

  TokenRequest(const TokenRequest&) = delete;
  TokenRequest& operator=(const TokenRequest&) = delete;

  ~TokenRequest();

  void InitiateRequest();

 private:
  // xdg_activation_token_v1_listener callbacks:
  static void OnTokenReceived(void* data,
                              xdg_activation_token_v1* activation_token,
                              const char* token);

  void OnTimeout();
  void OnDone(std::string token);

  const raw_ptr<xdg_activation_v1> xdg_activation_;
  const raw_ptr<WaylandConnection> connection_;
  wl::Object<xdg_activation_token_v1> token_;
  // Callback to invoke when the server responds or timeout
  // occurs.
  base::nix::XdgActivationTokenCallback callback_;
  base::OneShotTimer timer_;
};

// static
constexpr char XdgActivation::kInterfaceName[];

// static
void XdgActivation::Instantiate(WaylandConnection* connection,
                                wl_registry* registry,
                                uint32_t name,
                                const std::string& interface,
                                uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->xdg_activation_)
    return;

  auto instance = wl::Bind<::xdg_activation_v1>(registry, name,
                                                std::min(version, kMaxVersion));
  if (!instance) {
    LOG(ERROR) << "Failed to bind " << kInterfaceName;
    return;
  }
  connection->xdg_activation_ =
      std::make_unique<XdgActivation>(std::move(instance), connection);
}

XdgActivation::XdgActivation(wl::Object<xdg_activation_v1> xdg_activation_v1,
                             WaylandConnection* connection)
    : xdg_activation_v1_(std::move(xdg_activation_v1)),
      connection_(connection) {
  base::nix::SetXdgActivationTokenCreator(base::BindRepeating(
      &XdgActivation::RequestNewToken, base::Unretained(this)));
}

XdgActivation::~XdgActivation() {
  base::nix::SetXdgActivationTokenCreator({});
}

void XdgActivation::Activate(wl_surface* surface,
                             const std::string& token) const {
  xdg_activation_v1_activate(xdg_activation_v1_.get(), token.c_str(), surface);
}

void XdgActivation::RequestNewToken(
    base::nix::XdgActivationTokenCallback callback) const {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // This is not guaranteed to be called from the UI thread always.
    // So post a task to avoid race conditions if the request queue is accessed
    // simultaneously from requests and completion callbacks and handle the case
    // where the call may be from a non-sequenced task.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&XdgActivation::RequestNewToken,
                                          weak_ptr_factory_.GetMutableWeakPtr(),
                                          std::move(callback)));
    return;
  }
  constexpr size_t kMaxQueueSize = 100;
  if (token_request_queue_.size() >= kMaxQueueSize) {
    LOG(WARNING) << "Max token request limit reached. "
                    "Will no longer enqueue requests.";
    std::move(callback).Run({});
    return;
  }
  token_request_queue_.push(std::make_unique<TokenRequest>(
      xdg_activation_v1_.get(), connection_,
      base::BindOnce(&XdgActivation::OnTokenRequestCompleted,
                     base::Unretained(const_cast<XdgActivation*>(this)),
                     std::move(callback))));
  // If the earlier activation request is still being served, store the
  // incoming request and try to initiate it after the current one is done.
  // Otherwise, initiate it immediately.
  if (token_request_queue_.size() == 1) {
    token_request_queue_.front()->InitiateRequest();
  }
}

void XdgActivation::OnTokenRequestCompleted(
    base::nix::XdgActivationTokenCallback callback,
    std::string token) {
  std::move(callback).Run(std::move(token));
  CHECK(!token_request_queue_.empty());
  token_request_queue_.pop();
  if (!token_request_queue_.empty()) {
    token_request_queue_.front()->InitiateRequest();
  }
}

XdgActivation::TokenRequest::TokenRequest(
    xdg_activation_v1* xdg_activation,
    WaylandConnection* connection,
    base::nix::XdgActivationTokenCallback callback)
    : xdg_activation_(xdg_activation),
      connection_(connection),
      callback_(std::move(callback)) {
  constexpr auto kMaxTokenRequestDelay = base::Milliseconds(500);
  timer_.Start(FROM_HERE, kMaxTokenRequestDelay,
               base::BindOnce(&XdgActivation::TokenRequest::OnTimeout,
                              base::Unretained(this)));
}

void XdgActivation::TokenRequest::InitiateRequest() {
  auto* const token = xdg_activation_v1_get_activation_token(xdg_activation_);
  if (!token) {
    LOG(WARNING) << "Could not get an XDG activation token!";
    OnDone({});
    return;
  }
  token_ = wl::Object<xdg_activation_token_v1>(token);
  static constexpr xdg_activation_token_v1_listener kXdgActivationListener = {
      .done = &OnTokenReceived};
  xdg_activation_token_v1_add_listener(token_.get(), &kXdgActivationListener,
                                       this);
  // The spec isn't clear about what types of surfaces should be used as
  // the requestor surface, but all implementations of xdg_activation_v1
  // known to date accept the currently keyboard focused surface for
  // activation. Update if needed once the upstream issue gets fixed:
  // https://gitlab.freedesktop.org/wayland/wayland-protocols/-/issues/129
  const WaylandWindow* const keyboard_focused_window =
      connection_->window_manager()->GetCurrentKeyboardFocusedWindow();
  wl_surface* const keyboard_focused_surface =
      keyboard_focused_window
          ? keyboard_focused_window->root_surface()->surface()
          : nullptr;
  if (keyboard_focused_surface) {
    xdg_activation_token_v1_set_surface(token_.get(), keyboard_focused_surface);
  }
  if (auto serial = connection_->serial_tracker().GetSerial(
          {wl::SerialType::kTouchPress, wl::SerialType::kMousePress,
           wl::SerialType::kMouseEnter, wl::SerialType::kKeyPress});
      serial.has_value()) {
    xdg_activation_token_v1_set_serial(token_.get(), serial->value,
                                       connection_->seat()->wl_object());
  }
  xdg_activation_token_v1_commit(token_.get());
}

void XdgActivation::TokenRequest::OnTimeout() {
  DCHECK(!callback_.is_null());
  DVLOG(1) << "token request timed out";
  // Run callback with an empty token if we did not receive a token from the
  // server yet.
  OnDone({});
}

void XdgActivation::TokenRequest::OnDone(std::string token) {
  // Cancel the timeout callback in case it hasn't timed out.
  timer_.Stop();
  // Move callback to local scope as the callback deletes this object.
  auto callback = std::move(callback_);
  std::move(callback).Run(std::move(token));
}

XdgActivation::TokenRequest::~TokenRequest() = default;

// static
void XdgActivation::TokenRequest::OnTokenReceived(
    void* data,
    xdg_activation_token_v1* activation_token,
    const char* token) {
  DVLOG(1) << "new token received from server: " << token;
  auto* const self = static_cast<XdgActivation::TokenRequest*>(data);
  CHECK(self);
  DCHECK(!self->callback_.is_null());
  DCHECK(self->timer_.IsRunning());
  self->OnDone(token);
}

}  // namespace ui

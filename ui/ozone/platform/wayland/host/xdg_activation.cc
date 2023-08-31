// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_activation.h"

#include <xdg-activation-v1-client-protocol.h>

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {
constexpr uint32_t kMaxVersion = 1;
}

using ActivationDoneCallback = base::OnceCallback<void(std::string token)>;

// Wraps the actual activation token.
class XdgActivation::Token {
 public:
  Token(wl::Object<xdg_activation_token_v1> token,
        wl_surface* surface,
        wl_seat* seat,
        absl::optional<wl::Serial> serial,
        ActivationDoneCallback callback);
  Token(const Token&) = delete;
  Token& operator=(const Token&) = delete;
  ~Token();

 private:
  static void OnDone(void* data,
                     xdg_activation_token_v1* activation_token,
                     const char* token);

  wl::Object<xdg_activation_token_v1> token_;

  ActivationDoneCallback callback_;
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
      connection_(connection) {}

XdgActivation::~XdgActivation() = default;

void XdgActivation::Activate(wl_surface* surface) const {
  // The spec isn't clear about what types of surfaces should be used as
  // the requestor surface, but all implementations of xdg_activation_v1
  // known to date accept the currently keyboard focused surface for
  // activation. Update if needed once the upstream issue gets fixed:
  // https://gitlab.freedesktop.org/wayland/wayland-protocols/-/issues/129
  const WaylandWindow* const keyboard_focused_window =
      connection_->window_manager()->GetCurrentKeyboardFocusedWindow();
  if (token_.get() != nullptr) {
    // If the earlier activation request is still being served, store the
    // incoming request and try to serve it after the current one is done.
    activation_queue_.emplace(surface);
    return;
  }

  wl_surface* const keyboard_focused_surface =
      keyboard_focused_window
          ? keyboard_focused_window->root_surface()->surface()
          : nullptr;

  auto* const token =
      xdg_activation_v1_get_activation_token(xdg_activation_v1_.get());
  if (!token) {
    LOG(WARNING) << "Could not get an XDG activation token!";
    return;
  }

  token_ = std::make_unique<Token>(
      wl::Object<xdg_activation_token_v1>(token), keyboard_focused_surface,
      connection_->seat()->wl_object(),
      connection_->serial_tracker().GetSerial(
          {wl::SerialType::kTouchPress, wl::SerialType::kMousePress,
           wl::SerialType::kMouseEnter, wl::SerialType::kKeyPress}),
      base::BindOnce(&XdgActivation::OnActivateDone,
                     weak_factory_.GetMutableWeakPtr(), surface));
}

void XdgActivation::OnActivateDone(wl_surface* surface, std::string token) {
  xdg_activation_v1_activate(xdg_activation_v1_.get(), token.c_str(), surface);
  token_.reset();
  if (!activation_queue_.empty()) {
    Activate(activation_queue_.front());
    activation_queue_.pop();
  }
}

XdgActivation::Token::Token(wl::Object<xdg_activation_token_v1> token,
                            wl_surface* surface,
                            wl_seat* seat,
                            absl::optional<wl::Serial> serial,
                            ActivationDoneCallback callback)
    : token_(std::move(token)), callback_(std::move(callback)) {
  static constexpr xdg_activation_token_v1_listener kXdgActivationListener = {
      .done = &OnDone};
  xdg_activation_token_v1_add_listener(token_.get(), &kXdgActivationListener,
                                       this);
  if (surface) {
    xdg_activation_token_v1_set_surface(token_.get(), surface);
  }
  if (serial) {
    xdg_activation_token_v1_set_serial(token_.get(), serial->value, seat);
  }
  xdg_activation_token_v1_commit(token_.get());
}

XdgActivation::Token::~Token() = default;

// static
void XdgActivation::Token::OnDone(void* data,
                                  xdg_activation_token_v1* activation_token,
                                  const char* token) {
  if (auto* const self = static_cast<XdgActivation::Token*>(data)) {
    DCHECK(!self->callback_.is_null());
    std::move(self->callback_).Run(token);
  }
}

}  // namespace ui

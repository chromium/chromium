// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_idle_notify.h"

#include <ext-idle-notify-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"

namespace ui {

namespace {

constexpr uint32_t kMinVersion = 1;

// After the system has gone idle, it will wait for this time before notifying
// us.  This reduces "jitter" of the idle/active state, but also adds some lag
// in responsiveness: by the time we are finally notified, it has already been
// idle for kIdleThresholdMs milliseconds.
constexpr uint64_t kIdleThresholdMs = 5000;

}  // namespace

// Wraps the actual handling of system notifications about the idle state.
class ExtIdleNotifier::Timeout {
 public:
  explicit Timeout(ext_idle_notification_v1* timeout);
  Timeout(const Timeout&) = delete;
  Timeout& operator=(const Timeout&) = delete;
  ~Timeout();

  // Returns the idle time.
  base::TimeDelta GetIdleTime() const;

 private:
  // ext_idle_notification_v1_listener callbacks:
  static void OnIdled(void* data, ext_idle_notification_v1* idle_notification);
  static void OnResumed(void* data,
                        ext_idle_notification_v1* idle_notification);

  wl::Object<ext_idle_notification_v1> timeout_;

  // Time when the system went into idle state.
  base::Time idle_timestamp_;
};

// static
constexpr char ExtIdleNotifier::kInterfaceName[];

// static
void ExtIdleNotifier::Instantiate(WaylandConnection* connection,
                                  wl_registry* registry,
                                  uint32_t name,
                                  const std::string& interface,
                                  uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->ext_idle_notifier_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto idle = wl::Bind<ext_idle_notifier_v1>(registry, name, kMinVersion);
  if (!idle) {
    LOG(ERROR) << "Failed to bind to ext_idle_notifier_v1 global";
    return;
  }
  connection->ext_idle_notifier_ =
      std::make_unique<ExtIdleNotifier>(idle.release(), connection);
}

ExtIdleNotifier::ExtIdleNotifier(ext_idle_notifier_v1* idle,
                                 WaylandConnection* connection)
    : idle_(idle), connection_(connection) {}

ExtIdleNotifier::~ExtIdleNotifier() = default;

std::optional<base::TimeDelta> ExtIdleNotifier::GetIdleTime() const {
  if (!connection_->seat()) {
    return std::nullopt;
  }
  if (!idle_timeout_) {
    idle_timeout_ =
        std::make_unique<Timeout>(ext_idle_notifier_v1_get_idle_notification(
            idle_.get(), kIdleThresholdMs, connection_->seat()->wl_object()));
  }
  return idle_timeout_->GetIdleTime();
}

ExtIdleNotifier::Timeout::Timeout(ext_idle_notification_v1* timeout)
    : timeout_(timeout) {
  static constexpr ext_idle_notification_v1_listener kIdleNotificationListener =
      {.idled = &OnIdled, .resumed = &OnResumed};
  ext_idle_notification_v1_add_listener(timeout, &kIdleNotificationListener,
                                        this);
}

ExtIdleNotifier::Timeout::~Timeout() = default;

base::TimeDelta ExtIdleNotifier::Timeout::GetIdleTime() const {
  if (idle_timestamp_.is_null()) {
    return base::Seconds(0);
  }
  return base::Time::Now() - idle_timestamp_;
}

// static
void ExtIdleNotifier::Timeout::OnIdled(
    void* data,
    ext_idle_notification_v1* idle_notification) {
  auto* self = static_cast<ExtIdleNotifier::Timeout*>(data);
  self->idle_timestamp_ =
      base::Time::Now() - base::Milliseconds(kIdleThresholdMs);
}

// static
void ExtIdleNotifier::Timeout::OnResumed(
    void* data,
    ext_idle_notification_v1* idle_notification) {
  auto* self = static_cast<ExtIdleNotifier::Timeout*>(data);
  self->idle_timestamp_ = {};
}

}  // namespace ui

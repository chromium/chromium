// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/org_kde_kwin_idle.h"

#include <idle-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"

namespace ui {

namespace {

constexpr uint32_t kMinVersion = 1;

// After the system has gone idle, it will wait for this time before notifying
// us.  This reduces "jitter" of the idle/active state, but also adds some lag
// in responsiveness: when we are finally notified that the idle state has come,
// it is already there for kIdleThresholdMs milliseconds.
constexpr uint64_t kIdleThresholdMs = 5000;

}  // namespace

// Wraps the actual handling of system notifications about the idle state.
class OrgKdeKwinIdle::Timeout {
 public:
  explicit Timeout(org_kde_kwin_idle_timeout* timeout);
  Timeout(const Timeout&) = delete;
  Timeout& operator=(const Timeout&) = delete;
  ~Timeout();

  // Returns the idle time.
  base::TimeDelta GetIdleTime() const;

 private:
  // org_kde_kwin_idle_timeout_listener callbacks:
  static void OnIdle(void* data, org_kde_kwin_idle_timeout* idle_timeout);
  static void OnResumed(void* data, org_kde_kwin_idle_timeout* idle_timeout);

  wl::Object<org_kde_kwin_idle_timeout> timeout_;

  // Time when the system went into idle state.
  base::Time idle_timestamp_;
};

// static
constexpr char OrgKdeKwinIdle::kInterfaceName[];

// static
void OrgKdeKwinIdle::Instantiate(WaylandConnection* connection,
                                 wl_registry* registry,
                                 uint32_t name,
                                 const std::string& interface,
                                 uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->org_kde_kwin_idle_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto idle = wl::Bind<org_kde_kwin_idle>(registry, name, kMinVersion);
  if (!idle) {
    LOG(ERROR) << "Failed to bind to org_kde_kwin_idle global";
    return;
  }
  connection->org_kde_kwin_idle_ =
      std::make_unique<OrgKdeKwinIdle>(idle.release(), connection);
}

OrgKdeKwinIdle::OrgKdeKwinIdle(org_kde_kwin_idle* idle,
                               WaylandConnection* connection)
    : idle_(idle), connection_(connection) {}

OrgKdeKwinIdle::~OrgKdeKwinIdle() = default;

std::optional<base::TimeDelta> OrgKdeKwinIdle::GetIdleTime() const {
  if (!connection_->seat()) {
    return std::nullopt;
  }
  if (!idle_timeout_) {
    idle_timeout_ =
        std::make_unique<Timeout>(org_kde_kwin_idle_get_idle_timeout(
            idle_.get(), connection_->seat()->wl_object(), kIdleThresholdMs));
  }
  return idle_timeout_->GetIdleTime();
}

OrgKdeKwinIdle::Timeout::Timeout(org_kde_kwin_idle_timeout* timeout)
    : timeout_(timeout) {
  static constexpr org_kde_kwin_idle_timeout_listener kIdleTimeoutListener = {
      .idle = &OnIdle, .resumed = &OnResumed};
  org_kde_kwin_idle_timeout_add_listener(timeout, &kIdleTimeoutListener, this);
}

OrgKdeKwinIdle::Timeout::~Timeout() = default;

base::TimeDelta OrgKdeKwinIdle::Timeout::GetIdleTime() const {
  if (idle_timestamp_.is_null()) {
    return base::Seconds(0);
  }
  return base::Time::Now() - idle_timestamp_;
}

// static
void OrgKdeKwinIdle::Timeout::OnIdle(void* data,
                                     org_kde_kwin_idle_timeout* idle_timeout) {
  auto* self = static_cast<OrgKdeKwinIdle::Timeout*>(data);
  self->idle_timestamp_ =
      base::Time::Now() - base::Microseconds(kIdleThresholdMs);
}

// static
void OrgKdeKwinIdle::Timeout::OnResumed(
    void* data,
    org_kde_kwin_idle_timeout* idle_timeout) {
  auto* self = static_cast<OrgKdeKwinIdle::Timeout*>(data);
  self->idle_timestamp_ = {};
}

}  // namespace ui

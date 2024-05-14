// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/org_gnome_mutter_idle_monitor.h"

#include "base/logging.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace ui {

namespace {

// After the system has gone idle, the D-Bus service will wait for this time
// before notifying us.  This reduces "jitter" of the idle/active state, but
// also adds some lag in responsiveness: when we are finally notified that the
// idle state has come, it is already there for kIdleThresholdMs milliseconds.
constexpr uint64_t kIdleThresholdMs = 5000;

constexpr char kInterface[] = "org.gnome.Mutter.IdleMonitor";
constexpr char kObjectPath[] = "/org/gnome/Mutter/IdleMonitor/Core";

constexpr char kMethodAddIdleWatch[] = "AddIdleWatch";
constexpr char kMethodAddUserActiveWatch[] = "AddUserActiveWatch";
constexpr char kMethodGetIdletime[] = "GetIdletime";
constexpr char kSignalWatchFired[] = "WatchFired";

}  // namespace

OrgGnomeMutterIdleMonitor::OrgGnomeMutterIdleMonitor()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          base::TaskTraits(base::MayBlock(),
                           base::TaskPriority::USER_VISIBLE,
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN))) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner = task_runner_;
  bus_ = base::MakeRefCounted<dbus::Bus>(options);
}

OrgGnomeMutterIdleMonitor::~OrgGnomeMutterIdleMonitor() = default;

std::optional<base::TimeDelta> OrgGnomeMutterIdleMonitor::GetIdleTime() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  switch (service_state_) {
    case ServiceState::kUnknown:
      service_state_ = ServiceState::kInitializing;
      {
        dbus::ObjectProxy* dbus_proxy = bus_->GetObjectProxy(
            DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
        dbus::MethodCall name_has_owner_call(DBUS_INTERFACE_DBUS,
                                             "NameHasOwner");
        dbus::MessageWriter writer(&name_has_owner_call);
        writer.AppendString(kInterface);
        dbus_proxy->CallMethod(
            &name_has_owner_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
            base::BindOnce(&OrgGnomeMutterIdleMonitor::OnServiceHasOwner,
                           weak_factory_.GetMutableWeakPtr()));
      }
      return base::Seconds(0);

    case ServiceState::kInitializing:
      return base::Seconds(0);

    case ServiceState::kWorking:
      if (idle_timestamp_.is_null())
        return base::Seconds(0);
      return base::Time::Now() - idle_timestamp_;

    case ServiceState::kNotAvailable:
      return std::nullopt;

    default:
      NOTREACHED_IN_MIGRATION();
      return std::nullopt;
  }
}

void OrgGnomeMutterIdleMonitor::OnServiceHasOwner(dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  dbus::MessageReader reader(response);
  bool owned = false;
  if (!response || !reader.PopBool(&owned) || !owned) {
    // Calling methods on a non-existent service will lead to a timeout rather
    // than an immediate error, so check for service existence first.
    LOG(WARNING) << kInterface << " D-Bus service does not exist";
    return Shutdown();
  }

  proxy_ = bus_->GetObjectProxy(kInterface, dbus::ObjectPath(kObjectPath));

  // Connect the WatchFired signal.
  proxy_->ConnectToSignal(
      kInterface, kSignalWatchFired,
      base::BindRepeating(&OrgGnomeMutterIdleMonitor::OnWatchFired,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&OrgGnomeMutterIdleMonitor::OnWatchFiredSignalConnected,
                     weak_factory_.GetWeakPtr()));
}

void OrgGnomeMutterIdleMonitor::OnWatchFiredSignalConnected(
    const std::string& interface,
    const std::string& signal,
    bool succeeded) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  DCHECK_EQ(interface, kInterface);
  DCHECK_EQ(signal, kSignalWatchFired);

  if (!succeeded) {
    LOG(WARNING) << "Cannot connect to " << kSignalWatchFired << " signal of "
                 << kInterface << " D-Bus service";
    return Shutdown();
  }

  // Add the Idle watch.
  dbus::MethodCall call(kInterface, kMethodAddIdleWatch);
  dbus::MessageWriter writer(&call);
  writer.AppendUint64(kIdleThresholdMs);
  proxy_->CallMethod(&call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::BindOnce(&OrgGnomeMutterIdleMonitor::OnAddIdleWatch,
                                    weak_factory_.GetWeakPtr()));
}

void OrgGnomeMutterIdleMonitor::OnAddIdleWatch(dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!response || !ReadWatchId(response, &idle_watch_id_)) {
    LOG(WARNING) << "Call to " << kMethodAddIdleWatch << " method of "
                 << kInterface << " D-Bus service failed";
    return Shutdown();
  }

  // Add the User Active watch.
  dbus::MethodCall call(kInterface, kMethodAddUserActiveWatch);
  proxy_->CallMethod(
      &call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OrgGnomeMutterIdleMonitor::OnAddUserActiveWatch,
                     weak_factory_.GetWeakPtr()));
}

void OrgGnomeMutterIdleMonitor::OnAddUserActiveWatch(dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!response || !ReadWatchId(response, &active_watch_id_)) {
    LOG(WARNING) << "Call to " << kMethodAddUserActiveWatch << " method of "
                 << kInterface << " D-Bus service failed";
    return Shutdown();
  }

  if (service_state_ == ServiceState::kInitializing) {
    // If we are still initialising, request the current idle time to have the
    // correct initial idle time.
    dbus::MethodCall call(kInterface, kMethodGetIdletime);
    proxy_->CallMethod(&call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&OrgGnomeMutterIdleMonitor::OnGetIdletime,
                                      weak_factory_.GetWeakPtr()));
  }
}

void OrgGnomeMutterIdleMonitor::OnGetIdletime(dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!response || !UpdateIdleTime(response)) {
    LOG(WARNING) << "Call to " << kMethodGetIdletime << " method of "
                 << kInterface << " D-Bus service failed";
    return Shutdown();
  }

  // Successfully initialised.
  service_state_ = ServiceState::kWorking;
}

void OrgGnomeMutterIdleMonitor::OnWatchFired(dbus::Signal* signal) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  DCHECK(signal);

  dbus::MessageReader reader(signal);
  uint32_t id;
  if (!reader.PopUint32(&id) || reader.HasMoreData()) {
    LOG(WARNING) << "Invalid signal received (unexpected payload)";
    return Shutdown();
  }

  if (id == idle_watch_id_) {
    idle_timestamp_ = base::Time::Now() - base::Milliseconds(kIdleThresholdMs);
    dbus::MethodCall call(kInterface, kMethodAddUserActiveWatch);
    proxy_->CallMethod(
        &call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OrgGnomeMutterIdleMonitor::OnAddUserActiveWatch,
                       weak_factory_.GetWeakPtr()));
  } else if (id == active_watch_id_) {
    idle_timestamp_ = base::Time{};
  }
}

bool OrgGnomeMutterIdleMonitor::UpdateIdleTime(dbus::Message* message) {
  if (!message)
    return false;

  dbus::MessageReader reader(message);
  uint64_t idletime;
  if (!reader.PopUint64(&idletime) || reader.HasMoreData())
    return false;
  idle_timestamp_ = base::Time::Now() - base::Milliseconds(idletime);
  return true;
}

bool OrgGnomeMutterIdleMonitor::ReadWatchId(dbus::Message* message,
                                            uint32_t* watch_id) {
  if (!message)
    return false;

  DCHECK(watch_id);

  dbus::MessageReader reader(message);
  return (reader.PopUint32(watch_id) && !reader.HasMoreData());
}

void OrgGnomeMutterIdleMonitor::Shutdown() {
  idle_timestamp_ = base::Time{};
  service_state_ = ServiceState::kNotAvailable;
}

}  // namespace ui

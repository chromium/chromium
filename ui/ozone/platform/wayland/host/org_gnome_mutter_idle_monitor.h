// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_GNOME_MUTTER_IDLE_MONITOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_GNOME_MUTTER_IDLE_MONITOR_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace dbus {
class Bus;
class Message;
class ObjectProxy;
class Response;
class Signal;
}  // namespace dbus

namespace ui {

// Wraps org.gnome.Mutter.IdleMonitor D-Bus system service.
class OrgGnomeMutterIdleMonitor {
 public:
  OrgGnomeMutterIdleMonitor();
  explicit OrgGnomeMutterIdleMonitor(scoped_refptr<dbus::Bus> bus);
  ~OrgGnomeMutterIdleMonitor();

  // Returns the idle time, or std::nullopt if the service is unavailable.
  std::optional<base::TimeDelta> GetIdleTime() const;

 private:
  enum class ServiceState {
    kUnknown,
    kInitializing,
    kWorking,
    kNotAvailable,
  };

  // Four On... methods below are called in chain, one after another, in the
  // initialisation sequence. Error at any step stops the initialisation and
  // puts the instance into disabled state.
  void OnServiceHasOwner(dbus::Response* response);
  void OnWatchFiredSignalConnected(const std::string& interface,
                                   const std::string& signal,
                                   bool succeeded);
  void OnAddIdleWatch(dbus::Response* response);
  void OnGetIdletime(dbus::Response* response);

  void OnAddUserActiveWatch(dbus::Response* response);

  // Called back by the D-Bus service in two cases:
  // 1) Some time after the system entered the idle state.
  // 2) After the system exited the idle state.
  void OnWatchFired(dbus::Signal* signal);

  void RequestIdleTime() const;
  void RequestUserActiveWatch();
  bool ReadWatchId(dbus::Message* message, uint32_t* watch_id);

  // Puts the instance into state of terminal unavailability so it no longer
  // interacts with the D-Bus service and returns std::nullopt.
  void Shutdown();

  // Current state of the instance.
  mutable ServiceState service_state_{ServiceState::kUnknown};
  // Time when the system went into idle state.
  base::Time idle_timestamp_;

  uint32_t idle_watch_id_ = 0;
  uint32_t active_watch_id_ = 0;
  mutable bool idle_check_in_flight_ = false;
  mutable base::TimeTicks last_idle_check_time_;

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  THREAD_CHECKER(main_thread_checker_);

  base::WeakPtrFactory<OrgGnomeMutterIdleMonitor> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_GNOME_MUTTER_IDLE_MONITOR_H_

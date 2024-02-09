// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_GNOME_MUTTER_IDLE_MONITOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_GNOME_MUTTER_IDLE_MONITOR_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
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
  ~OrgGnomeMutterIdleMonitor();

  // Returns the idle time.
  //
  // If called on the instance that is off, will return 0.
  std::optional<base::TimeDelta> GetIdleTime() const;

 private:
  enum class ServiceState {
    kUnknown,
    kInitializing,
    kWorking,
    kNotAvailable,
  };

  // Five On... methods below are called in chain, one after another, in the
  // initialisation sequence.  Error at any step stops the initialisation and
  // puts the instance into disabled state.
  void OnServiceHasOwner(dbus::Response* response);
  void OnWatchFiredSignalConnected(const std::string& interface,
                                   const std::string& signal,
                                   bool succeeded);
  void OnAddIdleWatch(dbus::Response* response);
  void OnAddUserActiveWatch(dbus::Response* response);
  void OnGetIdletime(dbus::Response* response);

  // Called back by the D-Bus service in two cases:
  // 1) Some time after the system entered the idle state.
  // 2) After the system exited the idle state.
  void OnWatchFired(dbus::Signal* signal);

  // Readers that unpack data from incoming messages.
  bool UpdateIdleTime(dbus::Message* message);
  bool ReadWatchId(dbus::Message* message, uint32_t* watch_id);

  // Puts the instance into state of terminal unavailability so it no longer
  // interacts with the D-Bus service and returns default zero idle time.
  void Shutdown();

  // Current state of the instance.
  mutable ServiceState service_state_{ServiceState::kUnknown};
  // Time when the system went into idle state.
  base::Time idle_timestamp_;

  uint32_t idle_watch_id_ = 0;
  uint32_t active_watch_id_ = 0;

  scoped_refptr<dbus::Bus> bus_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  THREAD_CHECKER(main_thread_checker_);

  base::WeakPtrFactory<OrgGnomeMutterIdleMonitor> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ORG_GNOME_MUTTER_IDLE_MONITOR_H_

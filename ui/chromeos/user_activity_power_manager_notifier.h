// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_
#define UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ui {

class UserActivityDetector;

// Notifies the power manager via D-Bus when the user is active.
class UI_CHROMEOS_EXPORT UserActivityPowerManagerNotifier
    : public InputDeviceEventObserver,
      public UserActivityObserver,
      public device::mojom::FingerprintObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  // Registers and unregisters itself as an observer of |detector| on
  // construction and destruction.
  UserActivityPowerManagerNotifier(
      UserActivityDetector* detector,
      mojo::PendingRemote<device::mojom::Fingerprint> fingerprint);

  UserActivityPowerManagerNotifier(const UserActivityPowerManagerNotifier&) =
      delete;
  UserActivityPowerManagerNotifier& operator=(
      const UserActivityPowerManagerNotifier&) = delete;

  ~UserActivityPowerManagerNotifier() override;

  // InputDeviceEventObserver implementation.
  void OnStylusStateChanged(ui::StylusState state) override;

  // UserActivityObserver implementation.
  void OnUserActivity(const Event* event) override;

  // fingerprint::mojom::FingerprintObserver:
  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;
  void OnRestarted() override;
  void OnStatusChanged(device::mojom::BiometricsManagerStatus status) override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

 private:
  // Notifies power manager that the user is active and activity type. No-op if
  // it is within 5 seconds from |last_notify_time_|.
  void MaybeNotifyUserActivity(
      power_manager::UserActivityType user_activity_type);

  raw_ptr<UserActivityDetector> detector_;  // not owned

  mojo::Remote<device::mojom::Fingerprint> fingerprint_;
  mojo::Receiver<device::mojom::FingerprintObserver>
      fingerprint_observer_receiver_{this};

  // Last time that the power manager was notified.
  base::TimeTicks last_notify_time_;

  // True after SuspendImminent has been received and when SuspendDone has not
  // been received.
  bool suspending_ = false;
};

}  // namespace ui

#endif  // UI_CHROMEOS_USER_ACTIVITY_POWER_MANAGER_NOTIFIER_H_

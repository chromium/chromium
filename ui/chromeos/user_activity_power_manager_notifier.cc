// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/user_activity_power_manager_notifier.h"

#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
namespace {

// Minimum number of seconds between notifications.
const int kNotifyIntervalSec = 5;

// Returns a UserActivityType describing |event|.
power_manager::UserActivityType GetUserActivityTypeForEvent(
    const Event* event) {
  if (!event || event->type() != ET_KEY_PRESSED)
    return power_manager::USER_ACTIVITY_OTHER;

  switch (static_cast<const KeyEvent*>(event)->key_code()) {
    case VKEY_BRIGHTNESS_DOWN:
      return power_manager::USER_ACTIVITY_BRIGHTNESS_DOWN_KEY_PRESS;
    case VKEY_BRIGHTNESS_UP:
      return power_manager::USER_ACTIVITY_BRIGHTNESS_UP_KEY_PRESS;
    case VKEY_VOLUME_DOWN:
      return power_manager::USER_ACTIVITY_VOLUME_DOWN_KEY_PRESS;
    case VKEY_VOLUME_MUTE:
      return power_manager::USER_ACTIVITY_VOLUME_MUTE_KEY_PRESS;
    case VKEY_VOLUME_UP:
      return power_manager::USER_ACTIVITY_VOLUME_UP_KEY_PRESS;
    default:
      return power_manager::USER_ACTIVITY_OTHER;
  }
}

}  // namespace

UserActivityPowerManagerNotifier::UserActivityPowerManagerNotifier(
    UserActivityDetector* detector,
    service_manager::Connector* connector)
    : detector_(detector) {
  detector_->AddObserver(this);
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  // Connector can be null in tests.
  if (connector) {
    // Treat fingerprint attempts as user activies to turn on the screen.
    // I.e., when user tried to use fingerprint to unlock.
    connector->Connect(device::mojom::kServiceName,
                       fingerprint_.BindNewPipeAndPassReceiver());
    fingerprint_->AddFingerprintObserver(
        fingerprint_observer_receiver_.BindNewPipeAndPassRemote());
  }
}

UserActivityPowerManagerNotifier::~UserActivityPowerManagerNotifier() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  detector_->RemoveObserver(this);
}

void UserActivityPowerManagerNotifier::OnStylusStateChanged(
    ui::StylusState state) {
  if (state == StylusState::REMOVED)
    MaybeNotifyUserActivity(power_manager::USER_ACTIVITY_OTHER);
}

void UserActivityPowerManagerNotifier::OnUserActivity(const Event* event) {
  MaybeNotifyUserActivity(GetUserActivityTypeForEvent(event));
}

void UserActivityPowerManagerNotifier::OnAuthScanDone(
    device::mojom::ScanResult scan_result,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  MaybeNotifyUserActivity(power_manager::USER_ACTIVITY_OTHER);
}

void UserActivityPowerManagerNotifier::OnSessionFailed() {}

void UserActivityPowerManagerNotifier::OnRestarted() {}

void UserActivityPowerManagerNotifier::OnEnrollScanDone(
    device::mojom::ScanResult scan_result,
    bool enroll_session_complete,
    int percent_complete) {
  MaybeNotifyUserActivity(power_manager::USER_ACTIVITY_OTHER);
}

void UserActivityPowerManagerNotifier::MaybeNotifyUserActivity(
    power_manager::UserActivityType user_activity_type) {
  base::TimeTicks now = base::TimeTicks::Now();
  // InSeconds() truncates rather than rounding, so it's fine for this
  // comparison.
  // powerd depends on this D-Bus call to track input events. When the
  // system is about to suspend or in dark resume, report user activity
  // immediately so that powerd can transition to full resume and turn the
  // display on as soon as possible. OnUserActivity calls are rate-limited by
  // the sender, so it's safe to always notify while we're suspending.
  if (suspending_ || last_notify_time_.is_null() ||
      (now - last_notify_time_).InSeconds() >= kNotifyIntervalSec) {
    chromeos::PowerManagerClient::Get()->NotifyUserActivity(user_activity_type);
    last_notify_time_ = now;
  }
}

void UserActivityPowerManagerNotifier::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  suspending_ = true;
}

void UserActivityPowerManagerNotifier::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  suspending_ = false;
}

}  // namespace ui

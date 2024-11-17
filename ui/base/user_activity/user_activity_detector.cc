// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/user_activity/user_activity_detector.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"

namespace ui {

namespace {

// Returns a string describing |event|.
std::string GetEventDebugString(const ui::Event* event) {
  std::string details = base::StringPrintf(
      "type=%d name=%s flags=%d time=%" PRId64,
      base::to_underlying(event->type()), event->GetName(), event->flags(),
      (event->time_stamp() - base::TimeTicks()).InMilliseconds());

  if (event->IsKeyEvent()) {
    details += base::StringPrintf(" key_code=%d",
        static_cast<const ui::KeyEvent*>(event)->key_code());
  } else if (event->IsMouseEvent() || event->IsTouchEvent() ||
             event->IsGestureEvent()) {
    details += base::StringPrintf(" location=%s",
        static_cast<const ui::LocatedEvent*>(
            event)->location().ToString().c_str());
  }

  return details;
}

}  // namespace

const int UserActivityDetector::kNotifyIntervalMs = 200;

// Too low and mouse events generated at the tail end of reconfiguration
// will be reported as user activity and turn the screen back on; too high
// and we'll ignore legitimate activity.
const int UserActivityDetector::kDisplayPowerChangeIgnoreMouseMs = 1000;

// static
UserActivityDetector* UserActivityDetector::Get() {
  static base::NoDestructor<UserActivityDetector> user_activity_detector;
  return user_activity_detector.get();
}

bool UserActivityDetector::HasObserver(
    const UserActivityObserver* observer) const {
  return observers_.HasObserver(observer);
}

void UserActivityDetector::AddObserver(UserActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void UserActivityDetector::RemoveObserver(UserActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UserActivityDetector::OnDisplayPowerChanging() {
  honor_mouse_events_time_ =
      GetCurrentTime() + base::Milliseconds(kDisplayPowerChangeIgnoreMouseMs);
}

void UserActivityDetector::HandleExternalUserActivity() {
  HandleActivity(nullptr);
}

void UserActivityDetector::DidProcessEvent(
    const PlatformEvent& platform_event) {
  std::unique_ptr<ui::Event> event(ui::EventFromNative(platform_event));
  ProcessReceivedEvent(event.get());
}

void UserActivityDetector::PlatformEventSourceDestroying() {
  PlatformEventSource* platform_event_source =
      PlatformEventSource::GetInstance();
  CHECK(platform_event_source);
  platform_event_source->RemovePlatformEventObserver(this);
}

void UserActivityDetector::ResetStateForTesting() {
  last_activity_name_.clear();
  last_activity_time_ = base::TimeTicks();
  last_observer_notification_time_ = base::TimeTicks();
  now_for_test_ = base::TimeTicks();
  honor_mouse_events_time_ = base::TimeTicks();
}

void UserActivityDetector::InitPlatformEventSourceObservationForTesting() {
  InitPlatformEventSourceObservation();
}

UserActivityDetector::UserActivityDetector() {
  InitPlatformEventSourceObservation();
}

UserActivityDetector::~UserActivityDetector() = default;

void UserActivityDetector::InitPlatformEventSourceObservation() {
  PlatformEventSource* platform_event_source =
      PlatformEventSource::GetInstance();
  CHECK(platform_event_source);
  platform_event_source->AddPlatformEventObserver(this);
}

base::TimeTicks UserActivityDetector::GetCurrentTime() const {
  return !now_for_test_.is_null() ? now_for_test_ : base::TimeTicks::Now();
}

void UserActivityDetector::ProcessReceivedEvent(const ui::Event* event) {
  if (!event)
    return;

  if (event->IsMouseEvent() || event->IsMouseWheelEvent()) {
    if (event->flags() & ui::EF_IS_SYNTHESIZED)
      return;
    if (!honor_mouse_events_time_.is_null()
        && GetCurrentTime() < honor_mouse_events_time_)
      return;
  }

  HandleActivity(event);
}

void UserActivityDetector::HandleActivity(const ui::Event* event) {
  base::TimeTicks now = GetCurrentTime();
  last_activity_time_ = now;
  last_activity_name_ = event ? event->GetName() : std::string();
  if (last_observer_notification_time_.is_null() ||
      (now - last_observer_notification_time_).InMillisecondsF() >=
      kNotifyIntervalMs) {
    if (VLOG_IS_ON(1) && event)
      VLOG(1) << "Reporting user activity: " << GetEventDebugString(event);
    observers_.Notify(&UserActivityObserver::OnUserActivity, event);
    last_observer_notification_time_ = now;
  }
}

}  // namespace ui

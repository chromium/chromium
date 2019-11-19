// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

#include <linux/input.h>
#include <stddef.h>
#include <stdint.h>

#include <bitset>
#include <memory>
#include <queue>
// See if we compile against new enough headers and add missing definition
// if the headers are too old.
#ifndef MT_TOOL_PALM
#define MT_TOOL_PALM 2
#endif

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/metrics/field_trial_params.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/touch_evdev_debug_buffer.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"

namespace ui {

class DeviceEventDispatcherEvdev;
class FalseTouchFinder;
struct InProgressTouchEvdev;

EVENTS_OZONE_EVDEV_EXPORT extern const base::Feature kEnableSingleCancelTouch;

class EVENTS_OZONE_EVDEV_EXPORT TouchEventConverterEvdev
    : public EventConverterEvdev {
 public:
  TouchEventConverterEvdev(base::ScopedFD fd,
                           base::FilePath path,
                           int id,
                           const EventDeviceInfo& devinfo,
                           SharedPalmDetectionFilterState* shared_palm_state,
                           DeviceEventDispatcherEvdev* dispatcher);
  ~TouchEventConverterEvdev() override;

  // EventConverterEvdev:
  bool HasTouchscreen() const override;
  bool HasPen() const override;
  gfx::Size GetTouchscreenSize() const override;
  int GetTouchPoints() const override;
  void OnEnabled() override;
  void OnDisabled() override;

  void DumpTouchEventLog(const char* filename) override;

  // Update touch event logging state
  void SetTouchEventLoggingEnabled(bool enabled) override;

  // Sets callback to enable/disable palm suppression.
  void SetPalmSuppressionCallback(
      const base::RepeatingCallback<void(bool)>& callback) override;

  // Unsafe part of initialization.
  virtual void Initialize(const EventDeviceInfo& info);

  static const char kHoldCountAtReleaseEventName[];
  static const char kHoldCountAtCancelEventName[];
  static const char kPalmFilterTimerEventName[];

 private:
  friend class MockTouchEventConverterEvdev;

  // Overidden from base::MessagePumpLibevent::FdWatcher.
  void OnFileCanReadWithoutBlocking(int fd) override;

  virtual void Reinitialize();

  void ProcessMultitouchEvent(const input_event& input);
  void EmulateMultitouchEvent(const input_event& input);
  void ProcessKey(const input_event& input);
  void ProcessAbs(const input_event& input);
  void ProcessSyn(const input_event& input);

  // Returns an EventType to dispatch for |touch|. Returns ET_UNKNOWN if an
  // event should not be dispatched.
  EventType GetEventTypeForTouch(const InProgressTouchEvdev& touch);

  void ReportTouchEvent(const InProgressTouchEvdev& event,
                        EventType event_type,
                        base::TimeTicks timestamp);
  void ReportEvents(base::TimeTicks timestamp);

  void ProcessTouchEvent(InProgressTouchEvdev* event,
                         base::TimeTicks timestamp);

  void UpdateTrackingId(int slot, int tracking_id);
  void ReleaseTouches();

  // Returns true if all touches were marked cancelled. Otherwise false.
  bool MaybeCancelAllTouches();
  bool IsPalm(const InProgressTouchEvdev& touch);

  // Normalize pressure value to [0, 1].
  float ScalePressure(int32_t value) const;

  int NextTrackingId();

  // Input device file descriptor.
  const base::ScopedFD input_device_fd_;

  // Dispatcher for events.
  DeviceEventDispatcherEvdev* const dispatcher_;

  // Set if we drop events in kernel (SYN_DROPPED) or in process.
  bool dropped_events_ = false;

  // Device has multitouch capability.
  bool has_mt_ = false;

  // Device supports pen input.
  bool has_pen_ = false;

  // Use BTN_LEFT instead of BT_TOUCH.
  bool quirk_left_mouse_button_ = false;

  // Pressure values.
  int pressure_min_;
  int pressure_max_;  // Used to normalize pressure values.

  // Input range for tilt.
  int tilt_x_min_;
  int tilt_x_range_;
  int tilt_y_min_;
  int tilt_y_range_;

  // Input range for x-axis.
  float x_min_tuxels_;
  float x_num_tuxels_;

  // Input range for y-axis.
  float y_min_tuxels_;
  float y_num_tuxels_;

  // The resolution of ABS_MT_TOUCH_MAJOR/MINOR might be different from the
  // resolution of ABS_MT_POSITION_X/Y. As we use the (position range, display
  // pixels) to resize touch event radius, we have to scale major/minor.
  float touch_major_scale_ = 1.0f;
  float touch_minor_scale_ = 1.0f;

  // Number of touch points reported by driver
  int touch_points_ = 0;

  // Maximum value of touch major axis
  int major_max_ = 0;

  // Tracking id counter.
  int next_tracking_id_ = 0;

  // Touch point currently being updated from the /dev/input/event* stream.
  size_t current_slot_ = 0;

  // Flag that indicates if the touch logging enabled or not.
  bool touch_logging_enabled_ = true;

  // In-progress touch points.
  std::vector<InProgressTouchEvdev> events_;

  // In progress touch points, from being held, along with the timestamp they
  // were held at.
  std::vector<std::queue<std::pair<InProgressTouchEvdev, base::TimeTicks>>>
      held_events_;

  // Finds touches that need to be filtered.
  std::unique_ptr<FalseTouchFinder> false_touch_finder_;

  // Finds touches that are palms with user software not just firmware.
  const std::unique_ptr<PalmDetectionFilter> palm_detection_filter_;

  // Records the recent touch events. It is used to fill the feedback reports
  TouchEventLogEvdev touch_evdev_debug_buffer_;

  // Callback to enable/disable palm suppression.
  base::RepeatingCallback<void(bool)> enable_palm_suppression_callback_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

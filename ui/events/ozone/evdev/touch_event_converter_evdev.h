// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

#include <linux/input.h>
#include <stddef.h>
#include <stdint.h>

#include <bitset>
#include <memory>
#include <ostream>
#include <queue>
#include <set>
// See if we compile against new enough headers and add missing definition
// if the headers are too old.
#include "base/memory/raw_ptr.h"

#ifndef MT_TOOL_PALM
#define MT_TOOL_PALM 2
#endif

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_pump_epoll.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/heatmap_palm_detector.h"
#include "ui/events/ozone/evdev/touch_evdev_debug_buffer.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/types/event_type.h"

namespace ui {

class DeviceEventDispatcherEvdev;
class FalseTouchFinder;
struct InProgressTouchEvdev;

COMPONENT_EXPORT(EVDEV) BASE_DECLARE_FEATURE(kEnableSingleCancelTouch);

class COMPONENT_EXPORT(EVDEV) TouchEventConverterEvdev
    : public EventConverterEvdev {
 public:
  TouchEventConverterEvdev(base::ScopedFD fd,
                           base::FilePath path,
                           int id,
                           const EventDeviceInfo& devinfo,
                           SharedPalmDetectionFilterState* shared_palm_state,
                           DeviceEventDispatcherEvdev* dispatcher);

  TouchEventConverterEvdev(const TouchEventConverterEvdev&) = delete;
  TouchEventConverterEvdev& operator=(const TouchEventConverterEvdev&) = delete;

  ~TouchEventConverterEvdev() override;

  static std::unique_ptr<TouchEventConverterEvdev> Create(
      base::ScopedFD fd,
      base::FilePath path,
      int id,
      const EventDeviceInfo& devinfo,
      SharedPalmDetectionFilterState* shared_palm_state,
      DeviceEventDispatcherEvdev* dispatcher);

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

  // Sets callback to report the latest stylus state.
  void SetReportStylusStateCallback(
      const ReportStylusStateCallback& callback) override;

  // Sets callback to get the latest stylus state.
  void SetGetLatestStylusStateCallback(
      const GetLatestStylusStateCallback& callback) override;

  // Unsafe part of initialization.
  virtual void Initialize(const EventDeviceInfo& info);

  std::ostream& DescribeForLog(std::ostream& os) const override;

  static const char kHoldCountAtReleaseEventName[];
  static const char kHoldCountAtCancelEventName[];
  static const char kPalmFilterTimerEventName[];
  static const char kPalmTouchCountEventName[];
  static const char kRepeatedTouchCountEventName[];
  static const char kTouchGapAfterStylusEventName[];
  static const char kTouchGapBeforeStylusEventName[];
  static const char kTouchTypeAfterStylusEventName[];
  static const char kTouchTypeBeforeStylusEventName[];
  static const char kTouchSessionCountEventName[];
  static const char kTouchSessionLengthEventName[];
  static const char kStylusSessionCountEventName[];
  static const char kStylusSessionLengthEventName[];

 private:
  struct CancelledTouch {
    const base::TimeTicks cancel_timestamp;
    const float start_x, start_y;

    CancelledTouch(base::TimeTicks cancel_timestamp,
                   float start_x,
                   float start_y)
        : cancel_timestamp(cancel_timestamp),
          start_x(start_x),
          start_y(start_y) {}

    bool operator==(const CancelledTouch& other) const {
      return cancel_timestamp == other.cancel_timestamp &&
             start_x == other.start_x && start_y == other.start_y;
    }

    bool operator<(const CancelledTouch& other) const {
      return std::tie(cancel_timestamp, start_x, start_y) <
             std::tie(other.cancel_timestamp, other.start_x, other.start_y);
    }
  };

  friend class MockTouchEventConverterEvdev;

  // Overridden from base::MessagePumpEpoll::FdWatcher.
  void OnFileCanReadWithoutBlocking(int fd) override;

  virtual void Reinitialize();

  void ProcessMultitouchEvent(const input_event& input);
  void EmulateMultitouchEvent(const input_event& input);
  void ProcessKey(const input_event& input);
  void ProcessAbs(const input_event& input);
  void ProcessSyn(const input_event& input);

  // Returns an EventType to dispatch for |touch|. Returns EventType::kUnknown
  // if an event should not be dispatched.
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

  bool SupportsOrientation() const;
  void UpdateRadiusFromTouchWithOrientation(InProgressTouchEvdev* event) const;

  int NextTrackingId();

  void UpdateSharedPalmState(base::TimeTicks timestamp);

  void DetectRepeatedTouch(base::TimeTicks timestamp);

  void RecordMetrics(base::TimeTicks timestamp);

  void RecordSession(base::TimeDelta session_length);

  // Input device file descriptor.
  const base::ScopedFD input_device_fd_;

  // Dispatcher for events.
  const raw_ptr<DeviceEventDispatcherEvdev> dispatcher_;

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

  // Orientation values.
  int orientation_min_;
  int orientation_max_;

  // Input range for tilt.
  int tilt_x_min_;
  int tilt_x_range_;
  int tilt_y_min_;
  int tilt_y_range_;

  // Resolution of x and y, used to normalize stylus x and y coord.
  int x_res_;
  int y_res_;

  // Input range for x-axis.
  float x_min_tuxels_;
  float x_num_tuxels_;

  // Input range for y-axis.
  float y_min_tuxels_;
  float y_num_tuxels_;

  // Resolution of tool_x and tool_y.
  int tool_x_res_;
  int tool_y_res_;

  // Input range for tool_x-axis.
  float tool_x_min_tuxels_;
  float tool_x_num_tuxels_;

  // Input range for tool_y-axis.
  float tool_y_min_tuxels_;
  float tool_y_num_tuxels_;

  // The resolution of ABS_MT_TOUCH_MAJOR/MINOR might be different from the
  // resolution of ABS_MT_POSITION_X/Y. As we use the (position range, display
  // pixels) to resize touch event radius, we have to scale major/minor.

  // When the major axis is X, we precompute the scale for x_radius/y_radius
  // from ABS_MT_TOUCH_MAJOR/ABS_MT_TOUCH_MINOR respectively.
  float x_scale_ = 0.5f;
  float y_scale_ = 0.5f;
  // Since the x and y resolution can differ, we pre-compute the
  // x_radius/y_radius scale from ABS_MT_TOUCH_MINOR/ABS_MT_TOUCH_MAJOR
  // resolution respectively when ABS_MT_ORIENTATION is rotated.
  float rotated_x_scale_ = 0.5f;
  float rotated_y_scale_ = 0.5f;

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

  // Callback to report latest stylus state, set only when HasPen.
  ReportStylusStateCallback report_stylus_state_callback_;

  // Callback to get latest stylus state, set only when HasMultitouch.
  GetLatestStylusStateCallback get_latest_stylus_state_callback_;

  // Do we mark a touch as palm when touch_major is the max?
  bool palm_on_touch_major_max_;

  // Do we mark a touch as palm when the tool type is marked as TOOL_TYPE_PALM ?
  bool palm_on_tool_type_palm_;

  // The start time of a touch session.
  std::optional<base::TimeTicks> session_start_time_ = std::nullopt;

  // Whether the last touch was detected as palm.
  bool last_touch_is_palm_ = false;

  // Stores recently-canceled touches, which are used to detect repeated
  // touches.
  std::set<CancelledTouch> cancelled_touches_;

  // A delay timer starts whenever a touch is reported and stops after 5s. If a
  // new touch is reported during the 5s, the timer will be reset and restarted.
  // When the timer finishes, it will record session metrics.
  base::OneShotTimer record_session_timer_;

  // Not owned!
  const raw_ptr<SharedPalmDetectionFilterState> shared_palm_state_;

  // Whether device supports hidraw spi and heatmap palm detection.
  bool support_heatmap_palm_detection_ = false;

  base::WeakPtrFactory<TouchEventConverterEvdev> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

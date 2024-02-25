// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_X11_DEVICE_DATA_MANAGER_X11_H_
#define UI_EVENTS_DEVICES_X11_DEVICE_DATA_MANAGER_X11_H_

#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/x11/events_devices_x11_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xinput.h"

namespace ui {

// CrOS touchpad metrics gesture types
enum GestureMetricsType {
  kGestureMetricsTypeNoisyGround = 0,
  kGestureMetricsTypeUnknown,
};

// A bitfield describing which scroll axes are enabled for a device.
enum ScrollType {
  SCROLL_TYPE_NO_SCROLL = 0,
  SCROLL_TYPE_HORIZONTAL = 1 << 0,
  SCROLL_TYPE_VERTICAL = 1 << 1,
};

// A class that extracts and tracks the input events data. It currently handles
// mouse, touchpad and touchscreen devices.
class EVENTS_DEVICES_X11_EXPORT DeviceDataManagerX11
    : public DeviceDataManager {
 public:
  // Enumerate additional data that one might be interested on an input event,
  // which are usually wrapped in X valuators. If you modify any of this,
  // make sure to update the kCachedAtoms data structure in the source file
  // and the k*Type[Start/End] constants used by IsCMTDataType and
  // IsTouchDataType.
  enum DataType {
    // Define the valuators used the CrOS CMT driver. Used by mice and CrOS
    // touchpads.
    DT_CMT_SCROLL_X = 0,   // Scroll amount on the X (horizontal) direction.
    DT_CMT_SCROLL_Y,       // Scroll amount on the Y (vertical) direction.
    DT_CMT_ORDINAL_X,      // Original (unaccelerated) value on the X direction.
                           // Can be used both for scrolls and flings.
    DT_CMT_ORDINAL_Y,      // Original (unaccelerated) value on the Y direction.
                           // Can be used both for scrolls and flings.
    DT_CMT_START_TIME,     // Gesture start time.
    DT_CMT_END_TIME,       // Gesture end time.
    DT_CMT_FLING_X,        // Fling amount on the X (horizontal) direction.
    DT_CMT_FLING_Y,        // Fling amount on the Y (vertical) direction.
    DT_CMT_FLING_STATE,    // The state of fling gesture (whether the user just
                           // started flinging or they tapped down).
    DT_CMT_METRICS_TYPE,   // Metrics type of the metrics gesture, which are
                           // used to wrap interesting patterns that we would
                           // like to track via the UMA system.
    DT_CMT_METRICS_DATA1,  // Complementary data 1 of the metrics gesture.
    DT_CMT_METRICS_DATA2,  // Complementary data 2 of the metrics gesture.
    DT_CMT_FINGER_COUNT,   // Finger counts in the current gesture. A same type
                           // of gesture can have very different meanings based
                           // on that (e.g. 2f scroll v.s. 3f swipe).

    // End of CMT data types.
    // Beginning of touch data types.

    // Define the valuators following the Multi-touch Protocol. Used by
    // touchscreen devices.
    DT_TOUCH_MAJOR,        // Length of the touch area.
    DT_TOUCH_MINOR,        // Width of the touch area.
    DT_TOUCH_ORIENTATION,  // Angle between the X-axis and the major axis of the
                           // touch area.
    DT_TOUCH_PRESSURE,     // Pressure of the touch contact.

    DT_TOUCH_POSITION_X,  // Touch X position.
    DT_TOUCH_POSITION_Y,  // Touch Y position.

    // NOTE for XInput MT: 'Tracking ID' is provided in every touch event to
    // track individual touch. 'Tracking ID' is an unsigned 32-bit value and
    // is increased for each new touch. It will wrap back to 0 when reaching
    // the numerical limit.
    DT_TOUCH_TRACKING_ID,  // ID of the touch point.

    // Kernel timestamp from touch screen (if available).
    DT_TOUCH_RAW_TIMESTAMP,

    // End of touch data types.
    // Beginning of stylus data types.
    DT_STYLUS_POSITION_X,  // Position in X-axis
    DT_STYLUS_POSITION_Y,  // Position in Y-axis
    DT_STYLUS_PRESSURE,    // Pressure of contact
    DT_STYLUS_TILT_X,      // Tilt in X-axis
    DT_STYLUS_TILT_Y,      // Tilt in Y-axis

    DT_LAST_ENTRY  // This must come last.
  };

  // A Device ID number that can be passed to InvalidateScrollClasses that
  // invalidates all devices.
  static constexpr auto kAllDevices = static_cast<x11::Input::DeviceId>(-1);

  // Data struct to store extracted data from an input event.
  typedef std::map<int, double> EventData;

  static void CreateInstance();

  // We use int because enums can be casted to ints but not vice versa.
  static bool IsCMTDataType(const int type);
  static bool IsTouchDataType(const int type);
  static bool IsStylusDataType(const int type);

  // Returns the DeviceDataManagerX11 singleton.
  static DeviceDataManagerX11* GetInstance();

  DeviceDataManagerX11(const DeviceDataManagerX11&) = delete;
  DeviceDataManagerX11& operator=(const DeviceDataManagerX11&) = delete;

  // Returns if XInput2 is available on the system.
  bool IsXInput2Available() const;

  // Updates the list of devices.
  void UpdateDeviceList(x11::Connection* connection);

  // For multitouch events we use slot number to distinguish touches from
  // different fingers. This function returns true if the associated slot
  // for |xiev| can be found and it is saved in |slot|, returns false if
  // no slot can be found.
  bool GetSlotNumber(const x11::Input::DeviceEvent& xiev, int* slot);

  // Check if an XI event contains data of the specified type.
  bool HasEventData(const x11::Event& xev, const DataType type) const;

  // Get all event data in one pass. We extract only data types that we know
  // about (defined in enum DataType). The data is not processed (e.g. not
  // filled in by cached values) as in GetEventData.
  void GetEventRawData(const x11::Event& xev, EventData* data);

  // Get a datum of the specified type. Return true and the value
  // is updated if the data is found, false and value unchanged if the data is
  // not found. In the case of MT-B/XI2.2, the value can come from a previously
  // cached one (see the comment above last_seen_valuator_).
  bool GetEventData(const x11::Event& xev, const DataType type, double* value);

  // Check if the event is an XI input event in the strict sense
  // (i.e. XIDeviceEvent). This rules out things like hierarchy changes,
  /// device changes, property changes and so on.
  bool IsXIDeviceEvent(const x11::Event& xev) const;

  // Check if the event comes from stylus devices.
  bool IsStylusXInputEvent(const x11::Event& xev) const;

  // Check if the event comes from touchpad devices.
  bool IsTouchpadXInputEvent(const x11::Event& xev) const;

  // Check if the event comes from devices running CMT driver or using
  // CMT valuators (e.g. mouses). Note that doesn't necessarily mean the event
  // is a CMT event (e.g. it could be a mouse pointer move).
  bool IsCMTDeviceEvent(const x11::Event& xev) const;

  // Check if the event contains information about a ScrollClass, and
  // report which scroll axes are contained in this event, defined by
  // ScrollType.
  int GetScrollClassEventDetail(const x11::Event& xev) const;

  // Check if the event comes from a device that has a ScrollClass, and
  // report which scroll axes it supports as a bit field, defined by
  // ScrollType.
  int GetScrollClassDeviceDetail(const x11::Event& xev) const;

  // Check if the event is one of the CMT gesture events (scroll, fling,
  // metrics etc.).
  bool IsCMTGestureEvent(const x11::Event& xev) const;

  // Returns true if the event is of the specific type, false if not.
  bool IsScrollEvent(const x11::Event& xev) const;
  bool IsFlingEvent(const x11::Event& xev) const;
  bool IsCMTMetricsEvent(const x11::Event& xev) const;

  // Returns true if the event has CMT start/end timestamps.
  bool HasGestureTimes(const x11::Event& xev) const;

  // Extract data from a scroll event (a motion event with the necessary
  // valuators). User must first verify the event type with IsScrollEvent.
  // Pointers shouldn't be NULL.
  void GetScrollOffsets(const x11::Event& xev,
                        float* x_offset,
                        float* y_offset,
                        float* x_offset_ordinal,
                        float* y_offset_ordinal,
                        int* finger_count);

  // Extract data from a scroll class event (smooth scrolling). User must
  // first verify the event type with GetScrollClassEventDetail.
  // Pointers shouldn't be NULL.
  void GetScrollClassOffsets(const x11::Event& xev,
                             double* x_offset,
                             double* y_offset);

  // Invalidate stored scroll class counters, since they can change when
  // pointing at other windows. If kAllDevices is specified, all devices are
  // invalidated.
  void InvalidateScrollClasses(x11::Input::DeviceId device_id);

  // Extract data from a fling event. User must first verify the event type
  // with IsFlingEvent. Pointers shouldn't be NULL.
  void GetFlingData(const x11::Event& xev,
                    float* vx,
                    float* vy,
                    float* vx_ordinal,
                    float* vy_ordinal,
                    bool* is_cancel);

  // Extract data from a CrOS metrics gesture event. User must first verify
  // the event type with IsCMTMetricsEvent. Pointers shouldn't be NULL.
  void GetMetricsData(const x11::Event& xev,
                      GestureMetricsType* type,
                      float* data1,
                      float* data2);

  // Returns the mapped button.
  int GetMappedButton(int button);

  // Updates button mapping. This is usually called when a MappingNotify event
  // is received.
  void UpdateButtonMap();

  // Extract the start/end timestamps from CMT events. User must first verify
  // the event with HasGestureTimes. Pointers shouldn't be NULL.
  void GetGestureTimes(const x11::Event& xev,
                       double* start_time,
                       double* end_time);

  // Normalize the data value on deviceid to fall into [0, 1].
  // *value = (*value - min_value_of_tp) / (max_value_of_tp - min_value_of_tp)
  // Returns true and sets the normalized value in|value| if normalization is
  // successful. Returns false and |value| is unchanged otherwise.
  bool NormalizeData(x11::Input::DeviceId deviceid,
                     const DataType type,
                     double* value);

  // Extract the range of the data type. Return true if the range is available
  // and written into min & max, false if the range is not available.
  bool GetDataRange(x11::Input::DeviceId deviceid,
                    const DataType type,
                    double* min,
                    double* max);

  // Sets up relevant valuator informations for device ids in the device lists.
  // This function is only for test purpose. It does not query the X server for
  // the actual device info, but rather inits the relevant valuator structures
  // to have safe default values for testing.
  void SetDeviceListForTest(const std::vector<int>& touchscreen,
                            const std::vector<int>& cmt_devices,
                            const std::vector<int>& other_devices);

  void SetValuatorDataForTest(x11::Input::DeviceEvent* devev,
                              DataType type,
                              double value);

  // Sets the keys which are still allowed on a disabled keyboard device.
  void SetDisabledKeyboardAllowedKeys(
      std::unique_ptr<std::set<KeyboardCode>> excepted_keys);

  // Disables and enables events from devices by device id.
  void DisableDevice(x11::Input::DeviceId deviceid);
  void EnableDevice(x11::Input::DeviceId deviceid);

  bool IsDeviceEnabled(x11::Input::DeviceId device_id) const;

  // Returns true if |native_event| should be blocked.
  bool IsEventBlocked(const x11::Event& xev);

  const std::vector<x11::Input::DeviceId>& master_pointers() const {
    return master_pointers_;
  }

 protected:
  // DeviceHotplugEventObserver:
  void OnKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices) override;

 private:
  // Information about scroll valuators
  struct ScrollInfo {
    struct AxisInfo {
      // The scroll valuator number of this scroll axis.
      int number;
      // The scroll increment; a value of n indicates n movement equals one
      // traditional scroll unit.
      double increment;
      // Current scroll position; used to find the difference between events.
      double position;
      // If true then scroll has been seen in this direction.
      bool seen;
    };

    AxisInfo vertical, horizontal;
  };

  // Information from XIValuatorClassInfo.
  struct ValuatorInfo {
    // The valuator number.
    int number = -1;
    // The valuator min value.
    double min = 0.0;
    // The valuator max value.
    double max = 0.0;
  };

  DeviceDataManagerX11();
  ~DeviceDataManagerX11() override;

  void InitializeValuatorsForTest(int deviceid,
                                  int start_valuator,
                                  int end_valuator,
                                  double min_value,
                                  double max_value);

  // Updates a device based on a Valuator class info.
  DataType UpdateValuatorClassDevice(
      const x11::Input::DeviceClass::Valuator& valuator_class_info,
      x11::Atom* atoms,
      uint16_t deviceid);

  // Updates a device based on a Scroll class info.
  void UpdateScrollClassDevice(
      const x11::Input::DeviceClass::Scroll& scroll_class_info,
      uint16_t deviceid);

  // Normalize the scroll amount according to the increment size.
  // *value /= increment
  // *value is expected to be 1 or -1.
  // Returns true and sets the normalized value in |value| if normalization is
  // successful. Returns false and |value| is unchanged otherwise.
  double ExtractAndUpdateScrollOffset(
      DeviceDataManagerX11::ScrollInfo::AxisInfo* axis,
      double valuator) const;

  static const int kMaxXIEventType = 32;
  static const int kMaxSlotNum = 10;

  // A quick lookup table for determining if events from the pointer device
  // should be processed.
  std::bitset<kMaxDeviceNum> cmt_devices_;
  std::bitset<kMaxDeviceNum> touchpads_;
  std::bitset<kMaxDeviceNum> stylus_;

  // List of the master pointer devices.
  std::vector<x11::Input::DeviceId> master_pointers_;

  // A quick lookup table for determining if events from the XI device
  // should be blocked.
  std::bitset<kMaxDeviceNum> blocked_devices_;

  // The set of keys allowed while the keyboard is blocked.
  std::unique_ptr<std::set<KeyboardCode>> blocked_keyboard_allowed_keys_;

  // Number of valuators on the specific device.
  int valuator_count_[kMaxDeviceNum];

  // Index table to find valuator number, min and max for DataType on the
  // specific device by valuator_lookup_[device_id][data_type].
  std::vector<ValuatorInfo> valuator_lookup_[kMaxDeviceNum];

  // Indicates if the user has disabled high precision scrolling support.
  bool high_precision_scrolling_disabled_;

  // Index table to find the horizontal and vertical scroll valuator
  // numbers, scroll increments and scroll position.
  ScrollInfo scroll_data_[kMaxDeviceNum];

  // Index table to find the DataType for valuator on the specific device
  // by data_type_lookup_[device_id][valuator].
  std::vector<int> data_type_lookup_[kMaxDeviceNum];

  // Table to keep track of the last seen value for the specified valuator for
  // a specified slot of a device. Defaults to 0 if the valuator for that slot
  // was not specified in an earlier event. With MT-B/XI2.2, valuators in an
  // XEvent are not reported if the values haven't changed from the previous
  // event. So it is necessary to remember these valuators so that chrome
  // doesn't think X/device doesn't know about the valuators. We currently
  // use this only on touchscreen devices.
  std::vector<double> last_seen_valuator_[kMaxDeviceNum][kMaxSlotNum];

  // Map that stores meta-data for blocked keyboards. This is needed to restore
  // devices when they are re-enabled.
  std::map<x11::Input::DeviceId, ui::KeyboardDevice> blocked_keyboard_devices_;

  std::vector<uint8_t> button_map_;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_X11_DEVICE_DATA_MANAGER_X11_H_

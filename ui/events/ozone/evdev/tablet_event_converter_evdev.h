// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TABLET_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_TABLET_EVENT_CONVERTER_EVDEV_H_

#include <ostream>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_epoll.h"
#include "ui/events/event.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"

struct input_event;

namespace ui {

class DeviceEventDispatcherEvdev;

class COMPONENT_EXPORT(EVDEV) TabletEventConverterEvdev
    : public EventConverterEvdev {
 public:
  TabletEventConverterEvdev(base::ScopedFD fd,
                            base::FilePath path,
                            int id,
                            CursorDelegateEvdev* cursor,
                            const EventDeviceInfo& info,
                            DeviceEventDispatcherEvdev* dispatcher);

  TabletEventConverterEvdev(const TabletEventConverterEvdev&) = delete;
  TabletEventConverterEvdev& operator=(const TabletEventConverterEvdev&) =
      delete;

  ~TabletEventConverterEvdev() override;

  // EventConverterEvdev:
  void OnFileCanReadWithoutBlocking(int fd) override;
  bool HasGraphicsTablet() const override;

  void ProcessEvents(const struct input_event* inputs, int count);

  std::ostream& DescribeForLog(std::ostream& os) const override;

 private:
  friend class MockTabletEventConverterEvdev;
  void ConvertKeyEvent(const input_event& input);
  void ConvertAbsEvent(const input_event& input);
  void DispatchMouseButton(const input_event& input);
  void UpdateCursor();

  // Flush events delimited by EV_SYN. This is useful for handling
  // non-axis-aligned movement properly.
  void FlushEvents(const input_event& input);

  // Input device file descriptor.
  const base::ScopedFD input_device_fd_;

  // Controller for watching the input fd.
  base::MessagePumpEpoll::FdWatchController controller_;

  // Shared cursor state.
  const raw_ptr<CursorDelegateEvdev> cursor_;

  // Dispatcher for events.
  const raw_ptr<DeviceEventDispatcherEvdev> dispatcher_;

  int y_abs_location_ = 0;
  int x_abs_location_ = 0;
  int x_abs_min_;
  int y_abs_min_;
  int x_abs_range_;
  int y_abs_range_;

  int tilt_x_min_;
  int tilt_x_range_;
  int tilt_y_min_;
  int tilt_y_range_;

  float tilt_x_ = 0.0f;
  float tilt_y_ = 0.0f;
  float pressure_ = 0.0f;
  int pressure_max_;

  // Bitfield of currently active tools, with BTN_TOOL_PEN in the least
  // significant bit up to BTN_TOOL_LENS in the most significant bit.
  uint8_t active_tools_ = 0;

  // Whether we need to move the cursor
  bool abs_value_dirty_ = false;

  // Set if we drop events in kernel (SYN_DROPPED) or in process.
  bool dropped_events_ = false;

  // Pen has only one side button
  bool one_side_btn_pen_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TABLET_EVENT_CONVERTER_EVDEV_H_

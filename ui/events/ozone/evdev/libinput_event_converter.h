// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBINPUT_EVENT_CONVERTER_H_
#define UI_EVENTS_OZONE_EVDEV_LIBINPUT_EVENT_CONVERTER_H_

#include <libinput.h>

#include <optional>
#include <ostream>

#include "ui/events/ozone/evdev/event_converter_evdev.h"

namespace ui {

class CursorDelegateEvdev;
class DeviceEventDispatcherEvdev;
class EventDeviceInfo;

// Use libinput (freedesktop.org/wiki/Software/libinput) to read events from
// old touchpads that are not supported by libgestures.
//
// Note that although libinput can read inputs from any number of devices, this
// implementation only attaches one device per libinput context.
class LibInputEventConverter : public EventConverterEvdev {
 public:
  // This class wraps the libinput_event struct from libinput library.
  class LibInputEvent {
   public:
    LibInputEvent(LibInputEvent&& other);
    explicit LibInputEvent(libinput_event* const event);
    LibInputEvent(const LibInputEvent& other) = delete;
    LibInputEvent& operator=(const LibInputEvent& other) = delete;
    ~LibInputEvent();

    libinput_event_pointer* PointerEvent() const;
    libinput_event_type Type() const;

   private:
    libinput_event* event_;
  };

  // This class wraps the libinput_device struct from libinput library.
  class LibInputDevice {
   public:
    LibInputDevice(int id, libinput_device* const device);
    LibInputDevice(LibInputDevice&& other);
    LibInputDevice(const LibInputDevice& other) = delete;
    LibInputDevice& operator=(const LibInputDevice& other) = delete;
    ~LibInputDevice();

    void ApplySettings(const InputDeviceSettingsEvdev& settings) const;

   private:
    std::string GetCapabilitiesString();

    void SetNaturalScrollEnabled(const bool enabled) const;
    void SetSensitivity(const int sensitivity) const;
    void SetTapToClickEnabled(const bool enabled) const;

    const int device_id_;
    libinput_device* device_;
  };

  // This class wraps the libinput struct from libinput library. Any operations
  // that uses libinput struct are implemented here.
  class LibInputContext {
   public:
    static std::optional<LibInputContext> Create();
    LibInputContext(LibInputContext&& other);
    LibInputContext(const LibInputContext& other) = delete;
    LibInputContext& operator=(const LibInputContext& other) = delete;
    ~LibInputContext();

    std::optional<LibInputEventConverter::LibInputDevice> AddDevice(
        int id,
        const base::FilePath& path) const;
    bool Dispatch() const;
    int Fd();
    std::optional<LibInputEventConverter::LibInputEvent> NextEvent() const;

   private:
    explicit LibInputContext(libinput* const li);

    static int OpenRestricted(const char* path, int flags, void* user_data);
    static void CloseRestricted(int fd, void* user_data);
    static void LogHandler(libinput* libinput,
                           enum libinput_log_priority priority,
                           const char* format,
                           va_list args);

    static constexpr libinput_interface interface_ = {OpenRestricted,
                                                      CloseRestricted};

    libinput* li_;
  };

  static std::unique_ptr<LibInputEventConverter> Create(
      const base::FilePath& path,
      int id,
      const EventDeviceInfo& devinfo,
      CursorDelegateEvdev* cursor,
      DeviceEventDispatcherEvdev* dispatcher);

  LibInputEventConverter(LibInputEventConverter::LibInputContext&& ctx,
                         const base::FilePath& path,
                         int id,
                         const EventDeviceInfo& devinfo,
                         CursorDelegateEvdev* cursor,
                         DeviceEventDispatcherEvdev* dispatcher);
  LibInputEventConverter(const LibInputEventConverter& other) = delete;
  LibInputEventConverter& operator=(const LibInputEventConverter& other) =
      delete;
  ~LibInputEventConverter() override;

  void ApplyDeviceSettings(const InputDeviceSettingsEvdev& settings) final;

  bool HasKeyboard() const final;

  bool HasMouse() const final;

  bool HasTouchpad() const final;

  bool HasTouchscreen() const final;

  std::ostream& DescribeForLog(std::ostream& os) const override;

 private:
  void OnFileCanReadWithoutBlocking(int fd) final;
  void HandleEvent(const LibInputEvent& event);
  void HandlePointerMotion(const LibInputEvent& evt);
  void HandlePointerButton(const LibInputEvent& evt);
  void HandlePointerAxis(const LibInputEvent& evt);
  base::TimeTicks Timestamp(const LibInputEvent& evt);

  DeviceEventDispatcherEvdev* const dispatcher_;
  CursorDelegateEvdev* const cursor_;

  const bool has_keyboard_;
  const bool has_mouse_;
  const bool has_touchpad_;
  const bool has_touchscreen_;

  const LibInputContext context_;
  const std::optional<LibInputDevice> device_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBINPUT_EVENT_CONVERTER_H_

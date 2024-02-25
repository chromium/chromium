// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_thread_evdev.h"
#include "ui/events/ozone/evdev/input_controller_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/ozone/public/system_input_injector.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

class CursorDelegateEvdev;
class DeviceManager;
class InputDeviceFactoryEvdev;
class InputDeviceFactoryEvdevProxy;
struct KeyboardDevice;
struct TouchpadDevice;
class SystemInputInjector;
class GamepadProviderOzone;
enum class DomCode : uint32_t;
enum class StylusState;

#if !defined(USE_EVDEV)
#error Missing dependency on ui/events/ozone/evdev
#endif

// Ozone events implementation for the Linux input subsystem ("evdev").
//
// This is a UI thread object, but creates its own thread for I/O. See
// InputDeviceFactoryEvdev for the I/O thread part.
class COMPONENT_EXPORT(EVDEV) EventFactoryEvdev : public DeviceEventObserver,
                                                  public PlatformEventSource {
 public:
  EventFactoryEvdev(CursorDelegateEvdev* cursor,
                    DeviceManager* device_manager,
                    KeyboardLayoutEngine* keyboard_layout_engine);

  EventFactoryEvdev(const EventFactoryEvdev&) = delete;
  EventFactoryEvdev& operator=(const EventFactoryEvdev&) = delete;

  ~EventFactoryEvdev() override;

  // Must be called before Init().
  void SetUserInputTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> user_input_task_runner);

  // Initialize. Must be called with a valid message loop.
  void Init();

  void WarpCursorTo(gfx::AcceleratedWidget widget,
                    const gfx::PointF& location);

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector();

  InputControllerEvdev* input_controller() { return &input_controller_; }

  // User input events.
  void DispatchKeyEvent(const KeyEventParams& params);
  void DispatchMouseMoveEvent(const MouseMoveEventParams& params);
  void DispatchMouseButtonEvent(const MouseButtonEventParams& params);
  void DispatchMouseWheelEvent(const MouseWheelEventParams& params);
  void DispatchPinchEvent(const PinchEventParams& params);
  void DispatchScrollEvent(const ScrollEventParams& params);
  void DispatchTouchEvent(const TouchEventParams& params);

  // Device lifecycle events.
  void DispatchKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping);
  void DispatchTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices);
  void DispatchMouseDevicesUpdated(const std::vector<InputDevice>& devices,
                                   bool has_mouse);
  void DispatchPointingStickDevicesUpdated(
      const std::vector<InputDevice>& devices);
  void DispatchTouchpadDevicesUpdated(
      const std::vector<TouchpadDevice>& devices,
      bool has_haptic_touchpad);
  void DispatchGraphicsTabletDevicesUpdated(
      const std::vector<InputDevice>& devices);
  void DispatchUncategorizedDevicesUpdated(
      const std::vector<InputDevice>& devices);
  void DispatchDeviceListsComplete();
  void DispatchStylusStateChanged(StylusState stylus_state);
  void DispatchMicrophoneMuteSwitchValueChanged(bool muted);
  void DispatchAnyKeysPressedUpdated(bool any);

  // Gamepad event and gamepad device event. These events are dispatched to
  // GamepadObserver through GamepadProviderOzone.
  void DispatchGamepadEvent(const GamepadEvent& event);
  void DispatchGamepadDevicesUpdated(
      const std::vector<GamepadDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping);

 protected:
  // DeviceEventObserver overrides:
  //
  // Callback for device add (on UI thread).
  void OnDeviceEvent(const DeviceEvent& event) override;

  // PlatformEventSource:
  void OnDispatcherListChanged() override;

 private:
  // Dispatch event via PlatformEventSource.
  void DispatchUiEvent(ui::Event* event);

  int NextDeviceId();

  // Device thread initialization.
  void StartThread();
  void OnThreadStarted(
      std::unique_ptr<InputDeviceFactoryEvdevProxy> input_device_factory);

  void NotifyMiceAndPointingSticksUpdated();

  // Used to uniquely identify input devices.
  int last_device_id_ = 0;

  // Interface for scanning & monitoring input devices.
  const raw_ptr<DeviceManager> device_manager_;  // Not owned.

  // Gamepad provider to dispatch gamepad events.
  const raw_ptr<GamepadProviderOzone> gamepad_provider_;

  // Proxy for input device factory (manages device I/O objects).
  // The real object lives on a different thread.
  std::unique_ptr<InputDeviceFactoryEvdevProxy> input_device_factory_proxy_;

  // Modifier key state (shift, ctrl, etc).
  EventModifiers modifiers_;

  // Mouse button map.
  MouseButtonMapEvdev mouse_button_map_;

  // Pointing stick (a.k.a. TrackPoint) button map.
  MouseButtonMapEvdev pointing_stick_button_map_;

  // Keyboard state.
  KeyboardEvdev keyboard_;

  // Cursor movement.
  const raw_ptr<CursorDelegateEvdev> cursor_;

  // Object for controlling input devices.
  InputControllerEvdev input_controller_;

  // Whether we've set up the device factory.
  bool initialized_ = false;

  // Thread for device I/O.
  EventThreadEvdev thread_;

  // Touch event id generator.
  SequentialIDGenerator touch_id_generator_;

  scoped_refptr<base::SingleThreadTaskRunner> user_input_task_runner_;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<EventFactoryEvdev> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_

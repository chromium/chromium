// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_TEST_UTIL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_TEST_UTIL_H_

#include <memory>

#include "ui/events/ozone/evdev/event_dispatch_callback.h"

namespace ui {

class CursorDelegateEvdev;
class DeviceManager;
class DeviceEventDispatcherEvdev;
class EventFactoryEvdev;
class KeyboardLayoutEngine;

std::unique_ptr<DeviceManager> CreateDeviceManagerForTest();

std::unique_ptr<EventFactoryEvdev> CreateEventFactoryEvdevForTest(
    CursorDelegateEvdev* cursor,
    DeviceManager* device_manager,
    KeyboardLayoutEngine* keyboard_layout_engine,
    const EventDispatchCallback& callback);

std::unique_ptr<DeviceEventDispatcherEvdev>
CreateDeviceEventDispatcherEvdevForTest(EventFactoryEvdev* event_factory);

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_TEST_UTIL_H_

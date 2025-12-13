// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_ios.h"

#import <GameController/GameController.h>

#include "base/memory/singleton.h"
#include "build/blink_buildflags.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

@interface MouseDeviceObserverIOS : NSObject

+ (MouseDeviceObserverIOS*)sharedInstance;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (void)start;
- (void)stop;

@end

@implementation MouseDeviceObserverIOS {
  size_t _startCount;
  size_t _connectedDeviceCount;
}

+ (MouseDeviceObserverIOS*)sharedInstance {
  static MouseDeviceObserverIOS* instance = nil;
  static dispatch_once_t onceToken = 0;
  dispatch_once(&onceToken, ^{
    instance = [[MouseDeviceObserverIOS alloc] init];
  });
  return instance;
}

- (BOOL)hasMouseDevice {
  return _connectedDeviceCount > 0;
}

- (void)start {
  if (++_startCount > 1) {
    return;
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(mouseDidConnect:)
             name:GCMouseDidConnectNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(mouseDidDisconnect:)
             name:GCMouseDidDisconnectNotification
           object:nil];
}

- (void)stop {
  if (!_startCount || --_startCount) {
    return;
  }

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:GCMouseDidConnectNotification
              object:nil];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:GCMouseDidDisconnectNotification
              object:nil];
}

- (void)mouseDidConnect:(__unused GCMouse*)notification {
  _connectedDeviceCount++;
  ui::InputDeviceObserverIOS::GetInstance()
      ->NotifyObserversDeviceConfigurationChanged([self hasMouseDevice]);
}

- (void)mouseDidDisconnect:(__unused GCMouse*)notification {
  _connectedDeviceCount--;
  ui::InputDeviceObserverIOS::GetInstance()
      ->NotifyObserversDeviceConfigurationChanged([self hasMouseDevice]);
}

@end

namespace ui {

InputDeviceObserverIOS::InputDeviceObserverIOS() = default;

InputDeviceObserverIOS::~InputDeviceObserverIOS() = default;

InputDeviceObserverIOS* InputDeviceObserverIOS::GetInstance() {
  return base::Singleton<
      InputDeviceObserverIOS,
      base::LeakySingletonTraits<InputDeviceObserverIOS>>::get();
}

void InputDeviceObserverIOS::AddObserver(
    ui::InputDeviceEventObserver* observer) {
  if (observers_.empty()) {
    [[MouseDeviceObserverIOS sharedInstance] start];
  }
  observers_.AddObserver(observer);
}

void InputDeviceObserverIOS::RemoveObserver(
    ui::InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    [[MouseDeviceObserverIOS sharedInstance] stop];
  }
}

void InputDeviceObserverIOS::NotifyObserversDeviceConfigurationChanged(
    bool has_mouse_device) {
  if (has_mouse_device_ == has_mouse_device) {
    return;
  }
  has_mouse_device_ = has_mouse_device;
  observers_.Notify(
      &ui::InputDeviceEventObserver::OnInputDeviceConfigurationChanged,
      InputDeviceEventObserver::kMouse);
}

}  // namespace ui

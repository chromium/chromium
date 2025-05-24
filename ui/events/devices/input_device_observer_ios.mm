// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_ios.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/scoped_native_library.h"
#include "build/blink_buildflags.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

@class BKSMousePointerDevice;

// The BKSMousePointerDeviceObserver and BKSMousePointerService classes are part
// of the private BackBoardServices.framework. The declarations are sourced from
// WebKit/Source/WebKit/Platform/spi/ios/BackBoardServicesSPI.h.
@protocol BKSMousePointerDeviceObserver <NSObject>
@optional
- (void)mousePointerDevicesDidChange:
    (NSSet<BKSMousePointerDevice*>*)mousePointerDevices;
@end

@interface BKSMousePointerService : NSObject
+ (BKSMousePointerService*)sharedInstance;
- (id)addPointerDeviceObserver:(id<BKSMousePointerDeviceObserver>)observer;
@end

// The MouseDeviceObserverIOS class is implemented in WebKit's
// Source/WebKit/UIProcess/ios/WKMouseDeviceObserver.mm and relies on the
// private BackBoardServices.framework. As a result, the current version is not
// shippable until it is replaced with a public API that Apple will release, as
// noted in crbug.com/379764624.
@interface MouseDeviceObserverIOS : NSObject <BKSMousePointerDeviceObserver>

+ (MouseDeviceObserverIOS*)sharedInstance;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (void)start;
- (void)stop;

@end

@implementation MouseDeviceObserverIOS {
  BOOL _hasMouseDevice;
  size_t _startCount;
  __strong id _token;
  dispatch_queue_t _deviceObserverTokenQueue;
}

+ (MouseDeviceObserverIOS*)sharedInstance {
  static MouseDeviceObserverIOS* instance = nil;
  static dispatch_once_t onceToken = 0;
  dispatch_once(&onceToken, ^{
    instance = [[MouseDeviceObserverIOS alloc] init];
  });
  return instance;
}

- (instancetype)init {
  if ((self = [super init])) {
    _deviceObserverTokenQueue = dispatch_queue_create(
        "MouseDeviceObserverIOS _deviceObserverTokenQueue",
        DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

- (void)start {
  if (++_startCount > 1) {
    return;
  }

  __weak MouseDeviceObserverIOS* weakSelf = self;
  dispatch_async(_deviceObserverTokenQueue, ^{
    __strong MouseDeviceObserverIOS* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    base::ScopedNativeLibrary library = base::ScopedNativeLibrary(
        base::FilePath("/System/Library/PrivateFrameworks/"
                       "BackBoardServices.framework/BackBoardServices"));
    DCHECK(library.is_valid());
    Class serviceClass = NSClassFromString(@"BKSMousePointerService");
    DCHECK(serviceClass);
    BKSMousePointerService* mousePointerInstance =
        (BKSMousePointerService*)[serviceClass sharedInstance];
    DCHECK(mousePointerInstance);
    strongSelf->_token =
        [mousePointerInstance addPointerDeviceObserver:strongSelf];
  });
}

- (void)stop {
  if (!_startCount || --_startCount) {
    return;
  }

  __weak MouseDeviceObserverIOS* weakSelf = self;
  dispatch_async(_deviceObserverTokenQueue, ^{
    __strong MouseDeviceObserverIOS* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    if (strongSelf->_token) {
      if ([strongSelf->_token respondsToSelector:@selector(invalidate)]) {
        [strongSelf->_token invalidate];
      }
      strongSelf->_token = nil;
    }
  });
}

#pragma mark - BKSMousePointerDeviceObserver handlers

- (void)mousePointerDevicesDidChange:
    (NSSet<BKSMousePointerDevice*>*)mousePointerDevices {
  BOOL hasMouseDevice = mousePointerDevices.count > 0;
  if (hasMouseDevice == _hasMouseDevice) {
    return;
  }
  _hasMouseDevice = hasMouseDevice;
  ui::InputDeviceObserverIOS::GetInstance()
      ->NotifyObserversDeviceConfigurationChanged(hasMouseDevice);
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

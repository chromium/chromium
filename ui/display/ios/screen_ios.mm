// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "build/ios_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/screen_base.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/native_ui_types.h"

namespace display {
namespace {

// Return all screens associated with scenes of the application.
NSArray<UIScreen*>* GetAllActiveScreens() {
#if BUILDFLAG(IS_IOS_APP_EXTENSION)
  return [NSArray<UIScreen*> array];
#else
  NSMutableSet<UIScreen*>* screens = [NSMutableSet set];
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    auto* window_scene = base::apple::ObjCCastStrict<UIWindowScene>(scene);
    for (UIWindow* window in window_scene.windows) {
      if (UIScreen* screen = window.screen) {
        [screens addObject:screen];
      }
    }
  }
  return [screens allObjects];
#endif
}

class ScreenIos : public ScreenBase {
 public:
  ScreenIos() {
    base::WeakPtr<ScreenIos> weak_this = weak_factory_.GetWeakPtr();
    auto update_block = ^(NSNotification* notification) {
      if (!base::SingleThreadTaskRunner::HasCurrentDefault()) {
        return;
      }
      // This notification comes before UIScreen can change its bounds so post a
      // task so the update occurs after the UIScreen has been updated.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ScreenIos::ScreenChanged, weak_this));
    };

    NSNotificationCenter* default_center = NSNotificationCenter.defaultCenter;
#if !BUILDFLAG(IS_IOS_TVOS)
    orientation_observer_ = [default_center
        addObserverForName:UIDeviceOrientationDidChangeNotification
                    object:nil
                     queue:nil
                usingBlock:update_block];
#endif
    window_key_observer_ =
        [default_center addObserverForName:UIWindowDidBecomeKeyNotification
                                    object:nil
                                     queue:nil
                                usingBlock:update_block];
    ScreenChanged();
  }

  ScreenIos(const ScreenIos&) = delete;
  ScreenIos& operator=(const ScreenIos&) = delete;

  ~ScreenIos() override {
    NSNotificationCenter* default_center = NSNotificationCenter.defaultCenter;
#if !BUILDFLAG(IS_IOS_TVOS)
    [default_center removeObserver:orientation_observer_];
#endif
    [default_center removeObserver:window_key_observer_];
  }

  gfx::Point GetCursorScreenPoint() override {
    NOTIMPLEMENTED();
    return gfx::Point(0, 0);
  }

  bool IsWindowUnderCursor(gfx::NativeWindow window) override {
    NOTIMPLEMENTED();
    return false;
  }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    NOTIMPLEMENTED();
    return gfx::NativeWindow();
  }

  int GetNumDisplays() const override {
    return std::max(static_cast<int>([GetAllActiveScreens() count]), 1);
  }

 private:
  void ScreenChanged() {
    UIScreen* screen = GetAllActiveScreens().firstObject;
    if (!screen) {
      return;
    }

    const int64_t display_id = 0;
    Display display(display_id, gfx::Rect(screen.bounds));
    CGFloat scale = [screen scale];

    if (Display::HasForceDeviceScaleFactor()) {
      scale = Display::GetForcedDeviceScaleFactor();
    }
    display.set_device_scale_factor(scale);

#if !BUILDFLAG(IS_IOS_TVOS)
    Display::Rotation rotation = Display::ROTATE_0;
    switch (UIDevice.currentDevice.orientation) {
      case UIDeviceOrientationPortrait:
      case UIDeviceOrientationFaceUp:
      case UIDeviceOrientationFaceDown:
      case UIDeviceOrientationUnknown:
        rotation = Display::ROTATE_0;
        break;
      case UIDeviceOrientationPortraitUpsideDown:
        rotation = Display::ROTATE_180;
        break;
      case UIDeviceOrientationLandscapeLeft:
        rotation = Display::ROTATE_90;
        break;
      case UIDeviceOrientationLandscapeRight:
        rotation = Display::ROTATE_270;
        break;
    }
    display.set_rotation(rotation);
    display.set_touch_support(Display::TouchSupport::AVAILABLE);
    display.set_accelerometer_support(Display::AccelerometerSupport::AVAILABLE);
    AddInternalDisplayId(display_id);
#endif

    ProcessDisplayChanged(display, true /* is_primary */);
  }

#if !BUILDFLAG(IS_IOS_TVOS)
  id __strong orientation_observer_;
#endif
  id __strong window_key_observer_;
  base::WeakPtrFactory<ScreenIos> weak_factory_{this};
};

}  // namespace

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView view) {
  return gfx::NativeWindow(view.Get().window);
}

Screen* CreateNativeScreen() {
  return new ScreenIos;
}

float GetInternalDisplayDeviceScaleFactor() {
#if BUILDFLAG(IS_IOS_APP_EXTENSION)
  return 1.0f;
#else
  UIScreen* screen = GetAllActiveScreens().firstObject;
  return screen ? screen.scale : 1.0f;
#endif
}

}  // namespace display

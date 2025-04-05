// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "build/ios_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/screen_base.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/native_widget_types.h"

namespace display {
namespace {
class ScreenNotification {
 public:
  virtual void ScreenChanged() = 0;
};
}  // namespace
}  // namespace display

@interface ScreenObserver : NSObject {
  raw_ptr<display::ScreenNotification> _notifier;
}
- (void)mainScreenChanged;
@end

@implementation ScreenObserver

- (instancetype)initWithNotifier:(display::ScreenNotification*)notifier {
  if ((self = [super init])) {
    _notifier = notifier;
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
#if !BUILDFLAG(IS_IOS_TVOS)
    [defaultCenter addObserver:self
                      selector:@selector(mainScreenChanged)
                          name:UIDeviceOrientationDidChangeNotification
                        object:nil];
#endif
    [defaultCenter addObserver:self
                      selector:@selector(mainScreenChanged)
                          name:UIWindowDidBecomeKeyNotification
                        object:nil];
  }

  return self;
}

- (void)mainScreenChanged {
  if (!base::SingleThreadTaskRunner::HasCurrentDefault()) {
    return;
  }
  // This notification comes before UIScreen can change its bounds so post a
  // task so the update occurs after the UIScreen has been updated.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&display::ScreenNotification::ScreenChanged,
                                base::Unretained(_notifier)));
}

@end

namespace display {
namespace {

class ScreenIos : public ScreenBase, public ScreenNotification {
 public:
  ScreenIos() {
    observer_ = [[ScreenObserver alloc] initWithNotifier:this];
    ScreenChanged();
  }

  ScreenIos(const ScreenIos&) = delete;
  ScreenIos& operator=(const ScreenIos&) = delete;

  void ScreenChanged() override {
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
  // Return all screens associated with scenes of the application.
  NSArray<UIScreen*>* GetAllActiveScreens() const {
#if BUILDFLAG(IS_IOS_APP_EXTENSION)
    return [NSArray<UIScreen*> array];
#else
    NSMutableSet<UIScreen*>* screens = [NSMutableSet set];
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
      UIWindowScene* windowScene =
          base::apple::ObjCCastStrict<UIWindowScene>(scene);
      UIScreen* screen = windowScene.keyWindow.screen;
      if (screen) {
        [screens addObject:screen];
      }
    }
    return [screens allObjects];
#endif
  }

  ScreenObserver* __strong observer_;
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
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    UIScreen* screen = windowScene.keyWindow.screen;
    if (screen) {
      return [screen scale];
    }
  }
  return 1.0f;
#endif
}

}  // namespace display

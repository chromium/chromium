// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/screen.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/CVDisplayLink.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/display/display.h"
#include "ui/display/display_change_notifier.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/mac/coordinate_conversion.h"

extern "C" {
Boolean CGDisplayUsesForceToGray(void);
}

namespace display {
namespace {

struct DisplayMac {
  const Display display;
  NSScreen* const __weak ns_screen;
};

NSScreen* GetMatchingScreen(const gfx::Rect& match_rect) {
  // Default to the monitor with the current keyboard focus, in case
  // |match_rect| is not on any screen at all.
  NSScreen* max_screen = NSScreen.mainScreen;
  int max_area = 0;

  for (NSScreen* screen in NSScreen.screens) {
    gfx::Rect monitor_area = gfx::ScreenRectFromNSRect(screen.frame);
    gfx::Rect intersection = gfx::IntersectRects(monitor_area, match_rect);
    int area = intersection.width() * intersection.height();
    if (area > max_area) {
      max_area = area;
      max_screen = screen;
    }
  }

  return max_screen;
}

const std::vector<Display> DisplaysFromDisplaysMac(
    const std::vector<DisplayMac>& displays_mac) {
  std::vector<Display> displays;

  for (auto const& display_mac : displays_mac) {
    displays.push_back(display_mac.display);
  }

  return displays;
}

DisplayMac BuildDisplayForScreen(NSScreen* screen) {
  TRACE_EVENT0("ui", "BuildDisplayForScreen");
  NSRect frame = screen.frame;

  CGDirectDisplayID display_id =
      [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];

  Display display(display_id, gfx::Rect(NSRectToCGRect(frame)));
  NSRect visible_frame = screen.visibleFrame;
  NSScreen* primary = NSScreen.screens.firstObject;

  // Convert work area's coordinate systems.
  if ([screen isEqual:primary]) {
    gfx::Rect work_area = gfx::Rect(NSRectToCGRect(visible_frame));
    work_area.set_y(frame.size.height - visible_frame.origin.y -
                    visible_frame.size.height);
    display.set_work_area(work_area);
  } else {
    display.set_bounds(gfx::ScreenRectFromNSRect(frame));
    display.set_work_area(gfx::ScreenRectFromNSRect(visible_frame));
  }

  // Compute device scale factor
  CGFloat scale = screen.backingScaleFactor;
  if (Display::HasForceDeviceScaleFactor())
    scale = Display::GetForcedDeviceScaleFactor();
  display.set_device_scale_factor(scale);

  // Examine the presence of HDR.
  bool enable_hdr = false;
  float hdr_max_lum_relative = 1.f;
  const float max_potential_edr_value =
      screen.maximumPotentialExtendedDynamicRangeColorComponentValue;
  const float max_edr_value =
      screen.maximumExtendedDynamicRangeColorComponentValue;
  if (max_potential_edr_value > 1.f) {
    enable_hdr = true;
#if defined(ARCH_CPU_X86_64)
    // Disable HDR on Intel laptop screens because performance is unacceptably
    // bad.
    // https://crbug.com/1402882
    if (CGDisplayIsBuiltin(display_id) && max_potential_edr_value <= 2.f) {
      enable_hdr = false;
    }
#endif
    if (enable_hdr) {
      hdr_max_lum_relative =
          std::max(kMinHDRCapableMaxLuminanceRelative, max_edr_value);
    }
  }

  // Compute DisplayColorSpaces.
  gfx::ICCProfile icc_profile;
  {
    CGColorSpaceRef cg_color_space = screen.colorSpace.CGColorSpace;
    if (cg_color_space) {
      base::apple::ScopedCFTypeRef<CFDataRef> cf_icc_profile(
          CGColorSpaceCopyICCData(cg_color_space));
      if (cf_icc_profile) {
        icc_profile =
            gfx::ICCProfile::FromData(CFDataGetBytePtr(cf_icc_profile.get()),
                                      CFDataGetLength(cf_icc_profile.get()));
      }
    }
  }
  gfx::DisplayColorSpaces display_color_spaces(icc_profile.GetColorSpace(),
                                               gfx::BufferFormat::BGRA_8888);
  if (HasForceDisplayColorProfile()) {
    if (Display::HasEnsureForcedColorProfile()) {
      if (display_color_spaces != display.GetColorSpaces()) {
        LOG(FATAL) << "The display's color space does not match the color "
                      "space that was forced by the command line. This will "
                      "cause pixel tests to fail.";
      }
    }
  } else {
    if (enable_hdr) {
      bool needs_alpha_values[] = {true, false};
      for (const auto& needs_alpha : needs_alpha_values) {
        display_color_spaces.SetOutputColorSpaceAndBufferFormat(
            gfx::ContentColorUsage::kHDR, needs_alpha,
            gfx::ColorSpace::CreateExtendedSRGB(), gfx::BufferFormat::RGBA_F16);
      }
      display_color_spaces.SetHDRMaxLuminanceRelative(hdr_max_lum_relative);
    }
    display.SetColorSpaces(display_color_spaces);
  }
  display_color_spaces.SetSDRMaxLuminanceNits(
      gfx::ColorSpace::kDefaultSDRWhiteLevel);

  if (enable_hdr) {
    display.set_color_depth(Display::kHDR10BitsPerPixel);
    display.set_depth_per_component(Display::kHDR10BitsPerComponent);
  } else {
    display.set_color_depth(Display::kDefaultBitsPerPixel);
    display.set_depth_per_component(Display::kDefaultBitsPerComponent);
  }
  display.set_is_monochrome(CGDisplayUsesForceToGray());

  // Query the display's refresh rate.
  if (@available(macos 12.0, *)) {
    // NSScreen.minimumRefreshInterval is available on macOS 12.0+
    double refresh_rate = 1.0 / screen.minimumRefreshInterval;
    display.set_display_frequency(refresh_rate);
  } else {
    // CVDisplayLink is available on macOS 10.4–15.0.
    CVDisplayLinkRef display_link = nullptr;
    if (CVDisplayLinkCreateWithCGDisplay(display_id, &display_link) ==
        kCVReturnSuccess) {
      DCHECK(display_link);
      CVTime cv_time =
          CVDisplayLinkGetNominalOutputVideoRefreshPeriod(display_link);
      if (!(cv_time.flags & kCVTimeIsIndefinite)) {
        double refresh_rate = (static_cast<double>(cv_time.timeScale) /
                               static_cast<double>(cv_time.timeValue));
        display.set_display_frequency(refresh_rate);
      }
      CVDisplayLinkRelease(display_link);
    }
  }

  // CGDisplayRotation returns a double. Display::SetRotationAsDegree will
  // handle the unexpected situations were the angle is not a multiple of 90.
  display.SetRotationAsDegree(static_cast<int>(CGDisplayRotation(display_id)));

  // TODO(crbug.com/40129700): Support multiple internal displays.
  // CGDisplayIsBuiltin may return -1 on [dis]connect; see crbug.com/1457025.
  if (CGDisplayIsBuiltin(display_id) == YES) {
    SetInternalDisplayIds({display_id});
  }

  display.set_label(base::SysNSStringToUTF8(screen.localizedName));

  return DisplayMac{display, screen};
}

DisplayMac BuildPrimaryDisplay() {
  return BuildDisplayForScreen(NSScreen.screens.firstObject);
}

std::vector<DisplayMac> BuildDisplaysFromQuartz() {
  TRACE_EVENT0("ui", "BuildDisplaysFromQuartz");

  // Don't just return all online displays.  This would include displays
  // that mirror other displays, which are not desired in this list.  It's
  // tempting to use the count returned by CGGetActiveDisplayList, but active
  // displays exclude sleeping displays, and those are desired.

  // It would be ridiculous to have this many displays connected, but
  // CGDirectDisplayID is just an integer, so supporting up to this many
  // doesn't hurt.
  CGDirectDisplayID online_displays[1024];
  CGDisplayCount online_display_count = 0;
  if (CGGetOnlineDisplayList(std::size(online_displays), online_displays,
                             &online_display_count) != kCGErrorSuccess) {
    return std::vector<DisplayMac>(1, BuildPrimaryDisplay());
  }

  using ScreenIdsToScreensMap = std::map<CGDirectDisplayID, NSScreen*>;
  ScreenIdsToScreensMap screen_ids_to_screens;
  for (NSScreen* screen in NSScreen.screens) {
    NSDictionary* screen_device_description = [screen deviceDescription];
    CGDirectDisplayID screen_id =
        [screen_device_description[@"NSScreenNumber"] unsignedIntValue];
    screen_ids_to_screens[screen_id] = screen;
  }

  std::vector<DisplayMac> displays_mac;
  for (CGDisplayCount online_display_index = 0;
       online_display_index < online_display_count; ++online_display_index) {
    CGDirectDisplayID online_display = online_displays[online_display_index];
    if (CGDisplayMirrorsDisplay(online_display) == kCGNullDirectDisplay) {
      // If this display doesn't mirror any other, include it in the list.
      // The primary display in a mirrored set will be counted, but those that
      // mirror it will not be.
      auto foundScreen = screen_ids_to_screens.find(online_display);
      if (foundScreen != screen_ids_to_screens.end()) {
        displays_mac.push_back(BuildDisplayForScreen(foundScreen->second));
      }
    }
  }

  return displays_mac.empty()
             ? std::vector<DisplayMac>(1, BuildPrimaryDisplay())
             : displays_mac;
}

// Returns the minimum Manhattan distance from |point| to corners of |screen|
// frame.
CGFloat GetMinimumDistanceToCorner(const NSPoint& point, NSScreen* screen) {
  NSRect frame = [screen frame];
  CGFloat distance =
      fabs(point.x - NSMinX(frame)) + fabs(point.y - NSMinY(frame));
  distance = std::min(
      distance, fabs(point.x - NSMaxX(frame)) + fabs(point.y - NSMinY(frame)));
  distance = std::min(
      distance, fabs(point.x - NSMinX(frame)) + fabs(point.y - NSMaxY(frame)));
  distance = std::min(
      distance, fabs(point.x - NSMaxX(frame)) + fabs(point.y - NSMaxY(frame)));
  return distance;
}

class ScreenMac : public Screen {
 public:
  ScreenMac() {
    UpdateDisplays();

    CGDisplayRegisterReconfigurationCallback(
        ScreenMac::DisplayReconfigurationCallBack, this);

    auto update_block = ^(NSNotification* notification) {
      OnNSScreensMayHaveChanged();
    };

    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    screen_color_change_observer_ =
        [center addObserverForName:NSScreenColorSpaceDidChangeNotification
                            object:nil
                             queue:nil
                        usingBlock:update_block];
    screen_params_change_observer_ = [center
        addObserverForName:NSApplicationDidChangeScreenParametersNotification
                    object:nil
                     queue:nil
                usingBlock:update_block];
  }

  ScreenMac(const ScreenMac&) = delete;
  ScreenMac& operator=(const ScreenMac&) = delete;

  ~ScreenMac() override {
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    [center removeObserver:screen_color_change_observer_];
    [center removeObserver:screen_params_change_observer_];

    CGDisplayRemoveReconfigurationCallback(
        ScreenMac::DisplayReconfigurationCallBack, this);
  }

  gfx::Point GetCursorScreenPoint() override {
    // Flip coordinates to gfx (0,0 in top-left corner) using primary screen.
    return gfx::ScreenPointFromNSPoint([NSEvent mouseLocation]);
  }

  bool IsWindowUnderCursor(gfx::NativeWindow native_window) override {
    NSWindow* window = native_window.GetNativeNSWindow();
    return [NSWindow windowNumberAtPoint:NSEvent.mouseLocation
               belowWindowWithWindowNumber:0] == window.windowNumber;
  }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    NOTIMPLEMENTED();
    return gfx::NativeWindow();
  }

  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override {
    const NSPoint ns_point = gfx::ScreenPointToNSPoint(point);

    // Note: [NSApp orderedWindows] doesn't include NSPanels.
    for (NSWindow* window in NSApp.orderedWindows) {
      if (ignore.count(window))
        continue;

      if (!window.onActiveSpace) {
        continue;
      }

      // NativeWidgetMac::Close() calls -orderOut: on NSWindows before actually
      // closing them.
      if (!window.visible) {
        continue;
      }

      if (NSPointInRect(ns_point, window.frame)) {
        return window;
      }
    }

    return nil;
  }

  int GetNumDisplays() const override { return displays_mac_.size(); }

  const std::vector<Display>& GetAllDisplays() const override {
    return displays_;
  }

  Display GetDisplayNearestWindow(
      gfx::NativeWindow native_window) const override {
    if (displays_.size() == 1)
      return displays_[0];

    NSWindow* window = native_window.GetNativeNSWindow();
    if (!window)
      return GetPrimaryDisplay();

    // Note the following line calls -[NSWindow
    // _bestScreenBySpaceAssignmentOrGeometry] which is quite expensive and
    // performs IPC with the window server process.
    NSScreen* match_screen = window.screen;

    if (!match_screen)
      return GetPrimaryDisplay();

    return GetCachedDisplayForScreen(match_screen);
  }

  Display GetDisplayNearestView(gfx::NativeView native_view) const override {
    NSView* view = native_view.GetNativeNSView();
    NSWindow* window = view.window;
    if (!window)
      return GetPrimaryDisplay();
    return GetDisplayNearestWindow(window);
  }

  Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    NSArray* screens = NSScreen.screens;
    if (screens.count <= 1) {
      return GetPrimaryDisplay();
    }

    NSPoint ns_point = NSPointFromCGPoint(point.ToCGPoint());
    NSScreen* primary = screens[0];
    ns_point.y = NSMaxY(primary.frame) - ns_point.y;
    for (NSScreen* screen in screens) {
      if (NSMouseInRect(ns_point, screen.frame, NO)) {
        return GetCachedDisplayForScreen(screen);
      }
    }

    NSScreen* nearest_screen = primary;
    CGFloat min_distance = CGFLOAT_MAX;
    for (NSScreen* screen in screens) {
      CGFloat distance = GetMinimumDistanceToCorner(ns_point, screen);
      if (distance < min_distance) {
        min_distance = distance;
        nearest_screen = screen;
      }
    }
    return GetCachedDisplayForScreen(nearest_screen);
  }

  // Returns the display that most closely intersects the provided bounds.
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override {
    NSScreen* match_screen = GetMatchingScreen(match_rect);
    return GetCachedDisplayForScreen(match_screen);
  }

  // Returns the primary display.
  Display GetPrimaryDisplay() const override {
    // Primary display is defined as the display with the menubar,
    // which is always at index 0.
    NSScreen* primary = NSScreen.screens.firstObject;
    Display display = GetCachedDisplayForScreen(primary);
    return display;
  }

  void AddObserver(DisplayObserver* observer) override {
    change_notifier_.AddObserver(observer);
  }

  void RemoveObserver(DisplayObserver* observer) override {
    change_notifier_.RemoveObserver(observer);
  }

  static void DisplayReconfigurationCallBack(CGDirectDisplayID display,
                                             CGDisplayChangeSummaryFlags flags,
                                             void* userInfo) {
    ScreenMac* screen_mac = static_cast<ScreenMac*>(userInfo);
    screen_mac->OnNSScreensMayHaveChanged();
  }

 private:
  // Updates the display data structures.
  void UpdateDisplays() {
    displays_mac_ = BuildDisplaysFromQuartz();

    std::vector<Display> displays = DisplaysFromDisplaysMac(displays_mac_);
    if (displays != displays_) {
      DISPLAY_LOG(EVENT) << "Displays updated, count: " << displays.size();
      for (const auto& display : displays) {
        DISPLAY_LOG(EVENT) << display.ToString();
      }
    }

    // Keep |displays_| in sync with |displays_mac_|. It would be better to have
    // only the |displays_mac_| data structure and generate an array of Displays
    // from it as needed but GetAllDisplays() is defined as returning a
    // reference. There are no restrictions on how long a caller to
    // GetAllDisplays() can hold onto the reference so we have to assume callers
    // expect the vector's contents to always reflect the current state of the
    // world. Therefore update |displays_| whenever we update |displays_mac_|.
    displays_ = std::move(displays);
  }

  Display GetCachedDisplayForScreen(NSScreen* screen) const {
    for (const DisplayMac& display_mac : displays_mac_) {
      if (display_mac.ns_screen == screen)
        return display_mac.display;
    }
    // In theory, this should not be reached, but in practice, on Catalina, it
    // has been observed that -[NSScreen screens] changes before any
    // notifications are received. See crbug.com/1021340 and crbug.com/1352564
    DISPLAY_LOG(DEBUG) << "-[NSScreen screens] changed before notification.";
    return BuildDisplayForScreen(screen).display;
  }

  void OnDelayedNotification() {
    // This can only be called `delayed_notification_new_displays_` is identical
    // to `displays_` except for HDR headroom.
    DCHECK_EQ(delayed_notification_new_displays_.size(), displays_.size());
    for (size_t i = 0; i < displays_.size(); ++i) {
      DCHECK(display::Display::EqualExceptForHdrHeadroom(
          displays_[i], delayed_notification_new_displays_[i]));
    }

    // Update `displays_` and send the notification.
    auto old_displays = std::move(displays_);
    displays_ = std::move(delayed_notification_new_displays_);
    delayed_notification_new_displays_.clear();
    change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
  }

  void OnNSScreensMayHaveChanged() {
    TRACE_EVENT0("ui", "OnNSScreensMayHaveChanged");

    auto old_displays = std::move(displays_);

    UpdateDisplays();

    // Determine if anything changed, and if anything besides HDR headroom
    // changed.
    bool all_displays_equal = true;
    bool all_displays_equal_except_hdr_headroom = true;
    if (displays_.size() != old_displays.size()) {
      all_displays_equal = false;
      all_displays_equal_except_hdr_headroom = false;
    } else {
      for (size_t i = 0; i < displays_.size(); ++i) {
        if (!display::Display::EqualExceptForHdrHeadroom(displays_[i],
                                                         old_displays[i])) {
          all_displays_equal = false;
          all_displays_equal_except_hdr_headroom = false;
          break;
        }
        if (displays_[i] != old_displays[i]) {
          all_displays_equal = false;
        }
      }
    }

    // If nothing changed, do no notifications.
    if (all_displays_equal) {
      return;
    }

#if defined(ARCH_CPU_X86_64)
    // HDR transitions on Intel can have extremely bad performance, so limit
    // their updates to 2 FPS.
    constexpr auto kMinimumHdrHeadroomUpdateInterval = base::Seconds(1 / 2.f);
#else
    // Allow HDR headroom updates at 12 FPS. Empirically, this is the minimum
    // framerate that doesn't feel janky.
    constexpr auto kMinimumHdrHeadroomUpdateInterval = base::Seconds(1 / 12.f);
#endif

    // If only HDR headroom changed, start a timer to do delayed notifications
    // (only if it has not already started).
    if (all_displays_equal_except_hdr_headroom) {
      delayed_notification_new_displays_ = std::move(displays_);
      displays_ = std::move(old_displays);
      if (!delayed_notification_timer_.IsRunning()) {
        delayed_notification_timer_.Start(
            FROM_HERE, kMinimumHdrHeadroomUpdateInterval,
            base::BindOnce(&ScreenMac::OnDelayedNotification,
                           weak_factory_.GetWeakPtr()));
      }
      return;
    }

    // Stop and delete any delayed notifications, because we're doing an update
    // now.
    delayed_notification_new_displays_.clear();
    delayed_notification_timer_.Stop();

    // Do the update.
    change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
  }

  // The displays currently attached to the device. Updated by
  // OnNSScreensMayHaveChanged.
  std::vector<DisplayMac> displays_mac_;

  std::vector<Display> displays_;

  // The observers notified by NSScreenColorSpaceDidChangeNotification and
  // NSApplicationDidChangeScreenParametersNotification.
  id __strong screen_color_change_observer_;
  id __strong screen_params_change_observer_;

  DisplayChangeNotifier change_notifier_;

  // If only the HDR headroom changed, throttle display notification changes to
  // avoid choppy performance. Start`delayed_notification_timer_` to call
  // OnDelayedNotification, which will update `displays_` to
  // `delayed_notification_new_displays_`.
  base::OneShotTimer delayed_notification_timer_;
  std::vector<Display> delayed_notification_new_displays_;
  base::WeakPtrFactory<ScreenMac> weak_factory_{this};
};

}  // namespace

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView native_view) {
  NSView* view = native_view.GetNativeNSView();
  return view.window;
}

Screen* CreateNativeScreen() {
  return new ScreenMac;
}

}  // namespace display

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/display.h"
#include "ui/display/display_change_notifier.h"
#include "ui/display/mac/display_link_mac.h"
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
  NSScreen* const ns_screen;  // weak
};

NSScreen* GetMatchingScreen(const gfx::Rect& match_rect) {
  // Default to the monitor with the current keyboard focus, in case
  // |match_rect| is not on any screen at all.
  NSScreen* max_screen = [NSScreen mainScreen];
  int max_area = 0;

  for (NSScreen* screen in [NSScreen screens]) {
    gfx::Rect monitor_area = gfx::ScreenRectFromNSRect([screen frame]);
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
    const std::vector<DisplayMac> displays_mac) {
  std::vector<Display> displays;

  for (auto const& display_mac : displays_mac) {
    displays.push_back(display_mac.display);
  }

  return displays;
}

// Mac OS < 10.15 does not have a good way to get the name for a particular
// display. This method queries IOService to try to find a display matching the
// product and vendor ID of the passed in Core Graphics display, and returns a
// CFDictionary created using IODisplayCreateInfoDictionary for that display.
// If multiple identical screens are present this might return the info for the
// wrong display.
//
// If no matching screen is found in IOService, this returns null.
base::ScopedCFTypeRef<CFDictionaryRef> GetDisplayInfoFromIOService(
    CGDirectDisplayID display_id) {
  const uint32_t cg_vendor_number = CGDisplayVendorNumber(display_id);
  const uint32_t cg_model_number = CGDisplayModelNumber(display_id);

  // If display is unknown or not connected to a monitor, return an empty
  // string.
  if (cg_vendor_number == kDisplayVendorIDUnknown ||
      cg_vendor_number == 0xFFFFFFFF) {
    return base::ScopedCFTypeRef<CFDictionaryRef>();
  }

  // IODisplayConnect is only supported in Intel-powered Macs. On ARM based
  // Macs this returns an empty list. Fortunately we only use this code when
  // the OS is older than 10.15, and those OS versions don't support ARM anyway.
  base::mac::ScopedIOObject<io_iterator_t> it;
  if (IOServiceGetMatchingServices(kIOMasterPortDefault,
                                   IOServiceMatching("IODisplayConnect"),
                                   it.InitializeInto()) != 0) {
    // This may happen if a desktop Mac is running headless.
    return base::ScopedCFTypeRef<CFDictionaryRef>();
  }

  base::ScopedCFTypeRef<CFDictionaryRef> found_display;
  while (auto service = base::mac::ScopedIOObject<io_service_t>(
             IOIteratorNext(it.get()))) {
    auto info =
        base::ScopedCFTypeRef<CFDictionaryRef>(IODisplayCreateInfoDictionary(
            service.get(), kIODisplayOnlyPreferredName));

    CFNumberRef vendorIDRef = base::mac::GetValueFromDictionary<CFNumberRef>(
        info.get(), CFSTR(kDisplayVendorID));
    CFNumberRef productIDRef = base::mac::GetValueFromDictionary<CFNumberRef>(
        info.get(), CFSTR(kDisplayProductID));
    if (!vendorIDRef || !productIDRef)
      continue;

    long long vendorID, productID;
    CFNumberGetValue(vendorIDRef, kCFNumberLongLongType, &vendorID);
    CFNumberGetValue(productIDRef, kCFNumberLongLongType, &productID);
    if (cg_vendor_number == vendorID && cg_model_number == productID)
      return info;
  }

  return base::ScopedCFTypeRef<CFDictionaryRef>();
}

// Extract the (localized) name from a dictionary created by
// IODisplayCreateInfoDictionary. If `info` is null, or if no names are found
// in the dictionary, this returns an empty string.
std::string DisplayNameFromDisplayInfo(
    base::ScopedCFTypeRef<CFDictionaryRef> info) {
  if (!info)
    return std::string();

  CFDictionaryRef names = base::mac::GetValueFromDictionary<CFDictionaryRef>(
      info.get(), CFSTR(kDisplayProductName));
  if (!names)
    return std::string();

  // The `names` dictionary maps locale strings to localized product names for
  // the display. Find a key in the returned dictionary that best matches the
  // current locale. Since this doesn't need to be perfect (display names are
  // unlikely to be localize), we use the number of initial matching characters
  // as an approximation for how well two locale strings match. This way
  // countries and variants are ignored if they don't exist in one or the other,
  // but taken into account if they are present in both. If no match is found,
  // the first entry is used.
  struct SearchContext {
    CFStringRef name = 0;
    int match_size = -1;
  } context;
  CFDictionaryApplyFunction(
      names,
      [](const void* key, const void* value, void* context) {
        SearchContext* result = static_cast<SearchContext*>(context);
        CFStringRef key_string = base::mac::CFCast<CFStringRef>(key);
        CFStringRef value_string = base::mac::CFCast<CFStringRef>(value);
        if (!key_string || !value_string)
          return;

        std::string locale = base::i18n::GetCanonicalLocale(
            base::SysCFStringRefToUTF8(key_string));
        std::string configured_locale = base::i18n::GetConfiguredLocale();
        int match = base::ranges::mismatch(locale, configured_locale).first -
                    locale.begin();
        if (match > result->match_size) {
          result->name = value_string;
        }
      },
      &context);

  if (!context.name)
    return std::string();
  return base::SysCFStringRefToUTF8(context.name);
}

DisplayMac BuildDisplayForScreen(NSScreen* screen) {
  TRACE_EVENT0("ui", "BuildDisplayForScreen");
  NSRect frame = [screen frame];

  CGDirectDisplayID display_id =
      [[screen deviceDescription][@"NSScreenNumber"] unsignedIntValue];

  Display display(display_id, gfx::Rect(NSRectToCGRect(frame)));
  NSRect visible_frame = [screen visibleFrame];
  NSScreen* primary = [[NSScreen screens] firstObject];

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
  CGFloat scale = [screen backingScaleFactor];
  if (Display::HasForceDeviceScaleFactor())
    scale = Display::GetForcedDeviceScaleFactor();
  display.set_device_scale_factor(scale);

  // Examine the presence of HDR.
  bool enable_hdr = false;
  float hdr_max_lum_relative = 1.f;
  if (@available(macOS 10.15, *)) {
    const float max_potential_edr_value =
        [screen maximumPotentialExtendedDynamicRangeColorComponentValue];
    const float max_edr_value =
        [screen maximumExtendedDynamicRangeColorComponentValue];
    if (max_potential_edr_value > 1.f) {
      enable_hdr = true;
      hdr_max_lum_relative =
          std::max(kMinHDRCapableMaxLuminanceRelative, max_edr_value);
    }
  }

  // Compute DisplayColorSpaces.
  gfx::ICCProfile icc_profile;
  {
    CGColorSpaceRef cg_color_space = [[screen colorSpace] CGColorSpace];
    if (cg_color_space) {
      base::ScopedCFTypeRef<CFDataRef> cf_icc_profile(
          CGColorSpaceCopyICCData(cg_color_space));
      if (cf_icc_profile) {
        icc_profile = gfx::ICCProfile::FromData(
            CFDataGetBytePtr(cf_icc_profile), CFDataGetLength(cf_icc_profile));
      }
    }
  }
  gfx::DisplayColorSpaces display_color_spaces(icc_profile.GetColorSpace(),
                                               gfx::BufferFormat::RGBA_8888);
  if (HasForceDisplayColorProfile()) {
    if (Display::HasEnsureForcedColorProfile()) {
      if (display_color_spaces != display.color_spaces()) {
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
    display.set_color_spaces(display_color_spaces);
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

  if (auto display_link = ui::DisplayLinkMac::GetForDisplay(display_id))
    display.set_display_frequency(display_link->GetRefreshRate());

  // CGDisplayRotation returns a double. Display::SetRotationAsDegree will
  // handle the unexpected situations were the angle is not a multiple of 90.
  display.SetRotationAsDegree(static_cast<int>(CGDisplayRotation(display_id)));

  // TODO(crbug.com/1078903): Support multiple internal displays.
  if (CGDisplayIsBuiltin(display_id))
    SetInternalDisplayIds({display_id});

  if (@available(macOS 10.15, *)) {
    display.set_label(base::SysNSStringToUTF8(screen.localizedName));
  } else {
    display.set_label(
        DisplayNameFromDisplayInfo(GetDisplayInfoFromIOService(display_id)));
  }

  return DisplayMac{display, screen};
}

DisplayMac BuildPrimaryDisplay() {
  return BuildDisplayForScreen([[NSScreen screens] firstObject]);
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
  for (NSScreen* screen in [NSScreen screens]) {
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

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    screen_color_change_observer_.reset(
        [[center addObserverForName:NSScreenColorSpaceDidChangeNotification
                             object:nil
                              queue:nil
                         usingBlock:update_block] retain]);
    screen_params_change_observer_.reset([[center
        addObserverForName:NSApplicationDidChangeScreenParametersNotification
                    object:nil
                     queue:nil
                usingBlock:update_block] retain]);
  }

  ScreenMac(const ScreenMac&) = delete;
  ScreenMac& operator=(const ScreenMac&) = delete;

  ~ScreenMac() override {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
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
    return [NSWindow windowNumberAtPoint:[NSEvent mouseLocation]
             belowWindowWithWindowNumber:0] == [window windowNumber];
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
    for (NSWindow* window : [NSApp orderedWindows]) {
      if (ignore.count(window))
        continue;

      if (![window isOnActiveSpace])
        continue;

      // NativeWidgetMac::Close() calls -orderOut: on NSWindows before actually
      // closing them.
      if (![window isVisible])
        continue;

      if (NSPointInRect(ns_point, [window frame]))
        return window;
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
    NSScreen* match_screen = [window screen];

    if (!match_screen)
      return GetPrimaryDisplay();

    return GetCachedDisplayForScreen(match_screen);
  }

  Display GetDisplayNearestView(gfx::NativeView native_view) const override {
    NSView* view = native_view.GetNativeNSView();
    NSWindow* window = [view window];
    if (!window)
      return GetPrimaryDisplay();
    return GetDisplayNearestWindow(window);
  }

  Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    NSArray* screens = [NSScreen screens];
    if ([screens count] <= 1)
      return GetPrimaryDisplay();

    NSPoint ns_point = NSPointFromCGPoint(point.ToCGPoint());
    NSScreen* primary = screens[0];
    ns_point.y = NSMaxY([primary frame]) - ns_point.y;
    for (NSScreen* screen in screens) {
      if (NSMouseInRect(ns_point, [screen frame], NO))
        return GetCachedDisplayForScreen(screen);
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
    NSScreen* primary = [[NSScreen screens] firstObject];
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

    // Keep |displays_| in sync with |displays_mac_|. It would be better to have
    // only the |displays_mac_| data structure and generate an array of Displays
    // from it as needed but GetAllDisplays() is defined as returning a
    // reference. There are no restrictions on how long a caller to
    // GetAllDisplays() can hold onto the reference so we have to assume callers
    // expect the vector's contents to always reflect the current state of the
    // world. Therefore update |displays_| whenever we update |displays_mac_|.
    displays_ = DisplaysFromDisplaysMac(displays_mac_);
  }

  Display GetCachedDisplayForScreen(NSScreen* screen) const {
    for (const DisplayMac& display_mac : displays_mac_) {
      if (display_mac.ns_screen == screen)
        return display_mac.display;
    }
    // In theory, this should not be reached, but in practice, on Catalina, it
    // has been observed that -[NSScreen screens] changes before any
    // notifications are received.
    // https://crbug.com/1021340.
    DLOG(ERROR) << "Value of -[NSScreen screens] changed before notification.";
    return BuildDisplayForScreen(screen).display;
  }

  void OnNSScreensMayHaveChanged() {
    TRACE_EVENT0("ui", "OnNSScreensMayHaveChanged");

    auto old_displays = std::move(displays_);

    UpdateDisplays();

    if (old_displays != displays_) {
      change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
    }
  }

  // The displays currently attached to the device. Updated by
  // OnNSScreensMayHaveChanged.
  std::vector<DisplayMac> displays_mac_;

  std::vector<Display> displays_;

  // The observers notified by NSScreenColorSpaceDidChangeNotification and
  // NSApplicationDidChangeScreenParametersNotification.
  base::scoped_nsobject<id> screen_color_change_observer_;
  base::scoped_nsobject<id> screen_params_change_observer_;

  DisplayChangeNotifier change_notifier_;
};

}  // namespace

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView native_view) {
  NSView* view = native_view.GetNativeNSView();
  return [view window];
}

Screen* CreateNativeScreen() {
  return new ScreenMac;
}

}  // namespace display

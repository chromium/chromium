// Copyright 2020 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/ios/ios_system_data_collector.h"

#include <sys/sysctl.h>
#include <sys/utsname.h>

#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#import <UIKit/UIKit.h>

#include "base/apple/mach_logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "util/mac/sysctl.h"
#include "util/misc/clock.h"

namespace {

template <typename T, void (T::*M)(void)>
void AddObserver(CFStringRef notification_name, T* observer) {
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      observer,
      [](CFNotificationCenterRef center,
         void* observer_vp,
         CFNotificationName name,
         const void* object,
         CFDictionaryRef userInfo) {
        T* observer = reinterpret_cast<T*>(observer_vp);
        (observer->*M)();
      },
      notification_name,
      nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

}  // namespace

namespace crashpad {
namespace internal {

IOSSystemDataCollector::IOSSystemDataCollector()
    : major_version_(0),
      minor_version_(0),
      patch_version_(0),
      build_(),
      machine_description_(),
      orientation_(0),
      processor_count_(0),
      cpu_vendor_(),
      has_next_daylight_saving_time_(false),
      is_daylight_saving_time_(false),
      standard_offset_seconds_(0),
      daylight_offset_seconds_(0),
      standard_name_(),
      daylight_name_(),
      initialization_time_ns_(ClockMonotonicNanoseconds()) {
  NSOperatingSystemVersion version =
      [[NSProcessInfo processInfo] operatingSystemVersion];
  major_version_ = base::saturated_cast<int>(version.majorVersion);
  minor_version_ = base::saturated_cast<int>(version.minorVersion);
  patch_version_ = base::saturated_cast<int>(version.patchVersion);
  processor_count_ =
      base::saturated_cast<int>([[NSProcessInfo processInfo] processorCount]);
  build_ = ReadStringSysctlByName("kern.osversion", false);
  bundle_identifier_ =
      base::SysNSStringToUTF8([[NSBundle mainBundle] bundleIdentifier]);
// If CRASHPAD_IS_IOS_APP_EXTENSION is defined, then the code is compiled with
// -fapplication-extension and can only be used in an app extension. Otherwise
// check at runtime whether the code is executing in an app extension or not.
#if defined(CRASHPAD_IS_IOS_APP_EXTENSION)
  is_extension_ = true;
#else
  is_extension_ = [[NSBundle mainBundle].bundlePath hasSuffix:@"appex"];
#endif

#if defined(ARCH_CPU_X86_64)
  cpu_vendor_ = ReadStringSysctlByName("machdep.cpu.vendor", false);
#endif
  uint32_t addressable_bits = 0;
  size_t len = sizeof(uint32_t);
  // `machdep.virtual_address_size` is the number of addressable bits in
  // userspace virtual addresses
  if (sysctlbyname(
          "machdep.virtual_address_size", &addressable_bits, &len, NULL, 0) !=
      0) {
    addressable_bits = 0;
  }
  address_mask_ = ~((1UL << addressable_bits) - 1);

#if TARGET_OS_SIMULATOR
  // TODO(justincohen): Consider adding board and model information to
  // |machine_description| as well (similar to MacModelAndBoard in
  // util/mac/mac_util.cc).
  const char* model = getenv("SIMULATOR_MODEL_IDENTIFIER");
  if (model == nullptr) {
    switch ([[UIDevice currentDevice] userInterfaceIdiom]) {
      case UIUserInterfaceIdiomPhone:
        model = "iPhone";
        break;
      case UIUserInterfaceIdiomPad:
        model = "iPad";
        break;
      default:
        model = "Unknown";
        break;
    }
  }
  machine_description_ = base::StringPrintf("iOS Simulator (%s)", model);
#elif TARGET_OS_IPHONE
  utsname uts;
  if (uname(&uts) == 0) {
    machine_description_ = uts.machine;
  }
#else
#error "Unexpected target type OS."
#endif

  InstallHandlers();
}

IOSSystemDataCollector::~IOSSystemDataCollector() {
  CFNotificationCenterRemoveEveryObserver(CFNotificationCenterGetLocalCenter(),
                                          this);
}

void IOSSystemDataCollector::OSVersion(int* major,
                                       int* minor,
                                       int* bugfix) const {
  *major = major_version_;
  *minor = minor_version_;
  *bugfix = patch_version_;
}

void IOSSystemDataCollector::InstallHandlers() {
  // Timezone.
  AddObserver<IOSSystemDataCollector,
              &IOSSystemDataCollector::SystemTimeZoneDidChangeNotification>(
      (__bridge CFStringRef)NSSystemTimeZoneDidChangeNotification, this);
  SystemTimeZoneDidChangeNotification();

  // Orientation.
  AddObserver<IOSSystemDataCollector,
              &IOSSystemDataCollector::OrientationDidChangeNotification>(
      (__bridge CFStringRef)UIDeviceOrientationDidChangeNotification, this);
  OrientationDidChangeNotification();

#if !defined(CRASHPAD_IS_IOS_APP_EXTENSION)
  // Foreground/Background. Extensions shouldn't use UIApplication*.
  if (!is_extension_) {
    AddObserver<
        IOSSystemDataCollector,
        &IOSSystemDataCollector::ApplicationDidChangeActiveNotification>(
        (__bridge CFStringRef)UIApplicationDidBecomeActiveNotification, this);
    AddObserver<
        IOSSystemDataCollector,
        &IOSSystemDataCollector::ApplicationDidChangeActiveNotification>(
        (__bridge CFStringRef)UIApplicationDidEnterBackgroundNotification,
        this);
    ApplicationDidChangeActiveNotification();
  }
#endif
}

void IOSSystemDataCollector::SystemTimeZoneDidChangeNotification() {
  NSTimeZone* time_zone = NSTimeZone.localTimeZone;
  NSDate* transition =
      [time_zone nextDaylightSavingTimeTransitionAfterDate:[NSDate date]];
  if (transition == nil) {
    has_next_daylight_saving_time_ = false;
    is_daylight_saving_time_ = false;
    standard_offset_seconds_ =
        base::saturated_cast<int>([time_zone secondsFromGMTForDate:transition]);
    standard_name_ = base::SysNSStringToUTF8([time_zone abbreviation]);
    daylight_offset_seconds_ = standard_offset_seconds_;
    daylight_name_ = standard_name_;
  } else {
    has_next_daylight_saving_time_ = true;
    is_daylight_saving_time_ = time_zone.isDaylightSavingTime;
    if (time_zone.isDaylightSavingTime) {
      standard_offset_seconds_ = base::saturated_cast<int>(
          [time_zone secondsFromGMTForDate:transition]);
      standard_name_ =
          base::SysNSStringToUTF8([time_zone abbreviationForDate:transition]);
      daylight_offset_seconds_ =
          base::saturated_cast<int>([time_zone secondsFromGMT]);
      daylight_name_ = base::SysNSStringToUTF8([time_zone abbreviation]);
    } else {
      standard_offset_seconds_ =
          base::saturated_cast<int>([time_zone secondsFromGMT]);
      standard_name_ = base::SysNSStringToUTF8([time_zone abbreviation]);
      daylight_offset_seconds_ = base::saturated_cast<int>(
          [time_zone secondsFromGMTForDate:transition]);
      daylight_name_ =
          base::SysNSStringToUTF8([time_zone abbreviationForDate:transition]);
    }
  }
}

void IOSSystemDataCollector::OrientationDidChangeNotification() {
  orientation_ =
      base::saturated_cast<int>([[UIDevice currentDevice] orientation]);
}

void IOSSystemDataCollector::ApplicationDidChangeActiveNotification() {
#if defined(CRASHPAD_IS_IOS_APP_EXTENSION)
  NOTREACHED();
#else
  dispatch_assert_queue_debug(dispatch_get_main_queue());
  bool old_active = active_;
  active_ = [UIApplication sharedApplication].applicationState ==
            UIApplicationStateActive;
  if (active_ != old_active && active_application_callback_) {
    active_application_callback_(active_);
  }
#endif
}

}  // namespace internal
}  // namespace crashpad

// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "base/mac/mach_logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"

namespace {

std::string ReadStringSysctlByName(const char* name) {
  size_t buf_len;
  if (sysctlbyname(name, nullptr, &buf_len, nullptr, 0) != 0) {
    PLOG(WARNING) << "sysctlbyname (size) " << name;
    return std::string();
  }

  if (buf_len == 0) {
    return std::string();
  }

  std::string value(buf_len - 1, '\0');
  if (sysctlbyname(name, &value[0], &buf_len, nullptr, 0) != 0) {
    PLOG(WARNING) << "sysctlbyname " << name;
    return std::string();
  }

  return value;
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
      daylight_name_() {
  NSOperatingSystemVersion version =
      [[NSProcessInfo processInfo] operatingSystemVersion];
  major_version_ = base::saturated_cast<int>(version.majorVersion);
  minor_version_ = base::saturated_cast<int>(version.minorVersion);
  patch_version_ = base::saturated_cast<int>(version.patchVersion);
  processor_count_ =
      base::saturated_cast<int>([[NSProcessInfo processInfo] processorCount]);
  build_ = ReadStringSysctlByName("kern.osversion");

#if defined(ARCH_CPU_X86_64)
  cpu_vendor_ = ReadStringSysctlByName("machdep.cpu.vendor");
#endif

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
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemDataCollector::SystemTimeZoneDidChangeNotificationHandler,
      reinterpret_cast<CFStringRef>(NSSystemTimeZoneDidChangeNotification),
      nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  SystemTimeZoneDidChangeNotification();

  // Orientation.
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemDataCollector::OrientationDidChangeNotificationHandler,
      reinterpret_cast<CFStringRef>(UIDeviceOrientationDidChangeNotification),
      nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  OrientationDidChangeNotification();
}

// static
void IOSSystemDataCollector::SystemTimeZoneDidChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  static_cast<IOSSystemDataCollector*>(observer)
      ->SystemTimeZoneDidChangeNotification();
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

// static
void IOSSystemDataCollector::OrientationDidChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  static_cast<IOSSystemDataCollector*>(observer)
      ->OrientationDidChangeNotification();
}

void IOSSystemDataCollector::OrientationDidChangeNotification() {
  orientation_ =
      base::saturated_cast<int>([[UIDevice currentDevice] orientation]);
}

}  // namespace internal
}  // namespace crashpad

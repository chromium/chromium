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

#ifndef CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_COLLECTOR_H_
#define CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_COLLECTOR_H_

#import <CoreFoundation/CoreFoundation.h>

#include <string>

namespace crashpad {
namespace internal {

//! \brief Used to collect system level data before a crash occurs.
class IOSSystemDataCollector {
 public:
  IOSSystemDataCollector();
  ~IOSSystemDataCollector();

  void OSVersion(int* major, int* minor, int* bugfix) const;
  std::string MachineDescription() const { return machine_description_; }
  int ProcessorCount() const { return processor_count_; }
  std::string Build() const { return build_; }
  std::string CPUVendor() const { return cpu_vendor_; }
  bool HasDaylightSavingTime() const { return has_next_daylight_saving_time_; }
  bool IsDaylightSavingTime() const { return is_daylight_saving_time_; }
  int StandardOffsetSeconds() const { return standard_offset_seconds_; }
  int DaylightOffsetSeconds() const { return daylight_offset_seconds_; }
  std::string StandardName() const { return standard_name_; }
  std::string DaylightName() const { return daylight_name_; }

  // Currently unused by minidump.
  int Orientation() const { return orientation_; }

 private:
  // Notification handlers.
  void InstallHandlers();
  static void SystemTimeZoneDidChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);
  void SystemTimeZoneDidChangeNotification();

  static void OrientationDidChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);
  void OrientationDidChangeNotification();

  int major_version_;
  int minor_version_;
  int patch_version_;
  std::string build_;
  std::string machine_description_;
  int orientation_;
  int processor_count_;
  std::string cpu_vendor_;
  bool has_next_daylight_saving_time_;
  bool is_daylight_saving_time_;
  int standard_offset_seconds_;
  int daylight_offset_seconds_;
  std::string standard_name_;
  std::string daylight_name_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_COLLECTOR_H_

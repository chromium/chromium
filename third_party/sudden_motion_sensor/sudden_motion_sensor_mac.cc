// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is based on the SMSLib library.
//
// SMSLib Sudden Motion Sensor Access Library
// Copyright (c) 2010 Suitable Systems
// All rights reserved.
//
// Developed by: Daniel Griscom
//               Suitable Systems
//               http://www.suitable.com
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal with the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimers.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimers in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the names of Suitable Systems nor the names of its
// contributors may be used to endorse or promote products derived from
// this Software without specific prior written permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
//
// For more information about SMSLib, see
//    <http://www.suitable.com/tools/smslib.html>
// or contact
//    Daniel Griscom
//    Suitable Systems
//    1 Centre Street, Suite 204
//    Wakefield, MA 01880
//    (781) 665-0053

#include "sudden_motion_sensor_mac.h"

#include <math.h>
#include <sys/sysctl.h>

#include <iterator>
#include <memory>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"

struct SuddenMotionSensor::GenericMacbookSensor {
  // Name of device to be read.
  const char* service_name;

  // Number of bytes of the axis data.
  int axis_size;

  // Default calibration value for zero g.
  float zero_g;

  // Default calibration value for one g (negative when axis is inverted).
  float one_g;

  // Kernel function index.
  unsigned int function;

  // Size of the sensor record to be sent/received.
  unsigned int record_size;
};

struct SuddenMotionSensor::AxisData {
  // Location of the first byte representing the axis in the sensor data.
  int index;

  // Axis inversion flag. The value changes often between models.
  bool inverted;
};

// Sudden Motion Sensor descriptor.
struct SuddenMotionSensor::SensorDescriptor {
  // Prefix of model to be tested.
  const char* model_name;

  // Board id of model, or nullptr if it doesn't matter.
  const char* board_id;

  // Axis-specific data (x,y,z order).
  AxisData axis[3];
};

// Typical sensor parameters in MacBook models.
const SuddenMotionSensor::GenericMacbookSensor
    SuddenMotionSensor::kGenericSensor = {
  "SMCMotionSensor", 2,
  0, 251,
  5, 40
};

// Supported sensor descriptors. Add entries here to enhance compatibility.
// Tested in order; place more specific entries before more general ones. (All
// non-tested entries from SMSLib have been removed.)
const SuddenMotionSensor::SensorDescriptor
    SuddenMotionSensor::kSupportedSensors[] = {
  // Tested by tommyw on a 13" MacBook.
  { "MacBook1,1",    nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by S.Selz. (via avi) on a 13" MacBook.
  { "MacBook2,1",    nullptr, { { 0, true  }, { 2, false }, { 4, true  } } },

  // Tested by verhees on a 13" MacBook.
  { "MacBook3,1",    nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by adlr on a 13" MacBook.
  { "MacBook4,1",    nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by thakis on a 13" MacBook.
  { "MacBook5,1",    nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by Adam Gerson (via avi) on a 13" MacBook.
  { "MacBook5,2",    nullptr, { { 0, false }, { 2, true  }, { 4, true  } } },

  // Tested by tommyw on a 13" MacBook.
  { "MacBook6,1",    nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by avi on a 13" MacBook.
  { "MacBook7,1",    nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Note:
  // - MacBook8,1 (12" MacBook, early 2015)
  // - MacBook9,1 (12" MacBook, early 2016)
  // - MacBook10,1 (12" MacBook, mid 2017)
  // have no accelerometer sensors.

  // Tested by crc on a 13" MacBook Air.
  { "MacBookAir1,1", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by sfiera, pjw on a 13" MacBook Air.
  { "MacBookAir2,1", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Note:
  // - MacBookAir3,1 (11" MacBook Air, late 2010)
  // - MacBookAir3,2 (13" MacBook Air, late 2010)
  // - MacBookAir4,1 (11" MacBook Air, mid 2011)
  // - MacBookAir4,2 (13" MacBook Air, mid 2011)
  // - MacBookAir5,1 (11" MacBook Air, mid 2012)
  // - MacBookAir5,2 (13" MacBook Air, mid 2012)
  // - MacBookAir6,1 (11" MacBook Air, mid 2013)
  // - MacBookAir6,2 (13" MacBook Air, mid 2013)
  // - MacBookAir7,1 (11" MacBook Air, early 2015)
  // - MacBookAir7,2 (13" MacBook Air, early 2015)
  // have no accelerometer sensors.

  // Tested by crc on a 15" MacBook Pro.
  { "MacBookPro1,1", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by Raul Cuza (via avi) on a 17" MacBook Pro.
  { "MacBookPro1,2", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by L.V. (via avi) on a 17" MacBook Pro.
  { "MacBookPro2,1", nullptr, { { 0, true  }, { 2, false }, { 4, true  } } },

  // Tested by leandrogracia on a 15" MacBook Pro.
  { "MacBookPro2,2", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // Tested by S.Som. (via avi) on a 17" MacBook Pro.
  { "MacBookPro3,1", "Mac-F42388C8",
                              { { 0, true  }, { 2, false }, { 4, true  } } },

  // Tested by leandrogracia on a 15" MacBook Pro.
  { "MacBookPro3,1", nullptr, { { 0, false }, { 2, true  }, { 4, true  } } },

  // Tested by leandrogracia on a 15" MacBook Pro.
  // Tested by Eric Shapiro (via avi) on a 17" MacBook Pro.
  { "MacBookPro4,1", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // MacBookPro5,1 handled by the generic case below.
  // Tested by leandrogracia on a 15" MacBook Pro.

  // MacBookPro5,2 handled by the generic case below.
  // Tested by S.Selz. (via avi) on a 17" MacBook Pro.

  // Tested by dmaclach on a 15" MacBook Pro.
  { "MacBookPro5,3", nullptr, { { 2, false }, { 0, false }, { 4, true  } } },

  // MacBookPro5,4 handled by the generic case below.
  // Tested by leandrogracia on a 15" MacBook Pro.

  // Tested by leandrogracia on a 13" MacBook Pro.
  { "MacBookPro5,5", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // MacBookPro6,1 handled by the generic case below.
  // Tested by khom, leadpipe on a 17" MacBook Pro.

  // Tested by leandrogracia on a 15" MacBook Pro.
  { "MacBookPro6,2", nullptr, { { 0, true  }, { 2, false }, { 4, true  } } },

  // Tested by leandrogracia on a 13" MacBook Pro.
  { "MacBookPro7,1", nullptr, { { 0, true  }, { 2, true  }, { 4, false } } },

  // MacBookPro8,1 handled by the generic case below.
  // Tested by avi on a 13" MacBook Pro.

  // MacBookPro8,2 handled by the generic case below.
  // Tested by avi on a 15" MacBook Pro.

  // MacBookPro8,3 handled by the generic case below.
  // Tested by avi on a 17" MacBook Pro.

  // MacBookPro9,1 handled by the generic case below.
  // Tested by avi on a 15" MacBook Pro.

  // MacBookPro9,2 handled by the generic case below.
  // Tested by avi on a 13" MacBook Pro.

  // Note:
  // - MacBookPro10,1 (15" MacBook Pro Retina, mid 2012)
  // has no accelerometer sensors.

  // MacBookPro10,2 handled by the generic case below.
  // Tested by avi on a 13" MacBook Pro with Retina display.

  // Note:
  // - MacBookPro11,1 (13" MacBook Pro, late 2013 - mid 2014)
  // - MacBookPro11,2 (15" MacBook Pro, late 2013 - mid 2014)
  // - MacBookPro11,3 (15" MacBook Pro, late 2013 - mid 2014)
  // - MacBookPro11,4 (15" MacBook Pro, mid 2015)
  // - MacBookPro11,5 (15" MacBook Pro, mid 2015)
  // - MacBookPro12,1 (13" MacBook Pro, early 2015)
  // - MacBookPro13,1 (13" MacBook Pro, late 2016)
  // - MacBookPro13,2 (13" MacBook Pro, late 2016)
  // - MacBookPro13,3 (15" MacBook Pro, late 2016)
  // - MacBookPro14,1 (13" MacBook Pro, mid 2017)
  // - MacBookPro14,2 (13" MacBook Pro, mid 2017)
  // - MacBookPro14,3 (15" MacBook Pro, mid 2017)
  // have no accelerometer sensors.

  // Generic MacBook accelerometer sensor data, to be used for future models
  // as well as models for which it is verified to be correct. Note that this
  // configuration may have problems with inverted axes when used generically
  // for untested models.
  { "",              nullptr, { { 0, false }, { 2, false }, { 4, false } } }
};

// Create a SuddenMotionSensor object and return nullptr if no valid sensor
// found.
SuddenMotionSensor* SuddenMotionSensor::Create() {
  std::unique_ptr<SuddenMotionSensor> accelerometer(new SuddenMotionSensor);
  return accelerometer->Init() ? accelerometer.release() : nullptr;
}

SuddenMotionSensor::~SuddenMotionSensor() {
  IOServiceClose(io_connection_);
}

SuddenMotionSensor::SuddenMotionSensor()
    : sensor_(nullptr),
      io_connection_(0) {
}

//  Retrieve per-axis accelerometer values.
bool SuddenMotionSensor::ReadSensorValues(float axes[3]) {
  DCHECK(sensor_);

  // Reset output record memory buffer.
  std::fill(output_record_.begin(), output_record_.end(), 0x00);

  // Read record data from memory.
  const size_t kInputSize = kGenericSensor.record_size;
  size_t output_size = kGenericSensor.record_size;

  if (IOConnectCallStructMethod(io_connection_, kGenericSensor.function,
      static_cast<const char *>(&input_record_[0]), kInputSize,
      &output_record_[0], &output_size) != KERN_SUCCESS) {
    return false;
  }

  // Calculate per-axis calibrated values.
  float axis_value[3];

  for (int i = 0; i < 3; ++i) {
    int sensor_value = 0;
    int size  = kGenericSensor.axis_size;
    int index = sensor_->axis[i].index;

    // Important Note: Little endian is assumed as this code is Mac-only
    //                 and PowerPC is currently not supported.
    memcpy(&sensor_value, &output_record_[index], size);

    sensor_value = ExtendSign(sensor_value, size);

    // Correct value using the current calibration.
    axis_value[i] = static_cast<float>(sensor_value - kGenericSensor.zero_g) /
                    kGenericSensor.one_g;

    // Make sure we reject any NaN or infinite values.
    if (!isfinite(axis_value[i]))
      return false;

    // Clamp value to the [-1, 1] range.
    if (axis_value[i] < -1.0)
      axis_value[i] = -1.0;
    else if (axis_value[i] > 1.0)
      axis_value[i] = 1.0;

    // Apply axis inversion.
    if (sensor_->axis[i].inverted)
      axis_value[i] = -axis_value[i];
  }

  axes[0] = axis_value[0];
  axes[1] = axis_value[1];
  axes[2] = axis_value[2];

  return true;
}

// Probe the local hardware looking for a supported sensor device
// and initialize an I/O connection to it.
bool SuddenMotionSensor::Init() {
  // Request model name from the kernel.
  char local_model[32];  // size from SMSLib
  size_t local_model_size = sizeof(local_model);
  int params[2] = { CTL_HW, HW_MODEL };
  if (sysctl(params, 2, local_model, &local_model_size, nullptr, 0) != 0)
    return false;

  const SensorDescriptor* sensor_candidate = nullptr;

  // Look for the current model in the supported sensor list.
  base::ScopedCFTypeRef<CFDataRef> board_id_data;
  const int kNumSensors = std::size(kSupportedSensors);

  for (int i = 0; i < kNumSensors; ++i) {
    // Check if the supported sensor model name is a prefix
    // of the local hardware model (empty names are accepted).
    const char* p1 = kSupportedSensors[i].model_name;
    for (const char* p2 = local_model; *p1 != '\0' && *p1 == *p2; ++p1, ++p2)
      continue;
    if (*p1 != '\0')
      continue;

    // Check the board id.
    if (kSupportedSensors[i].board_id) {
      if (!board_id_data.get()) {
        CFMutableDictionaryRef dict =
            IOServiceMatching("IOPlatformExpertDevice");
        if (!dict)
          continue;

        io_service_t platform_expert =
            IOServiceGetMatchingService(kIOMasterPortDefault, dict);
        if (!platform_expert)
          continue;

        board_id_data.reset((CFDataRef)
            IORegistryEntryCreateCFProperty(platform_expert,
                                            CFSTR("board-id"),
                                            kCFAllocatorDefault,
                                            0));
        IOObjectRelease(platform_expert);
        if (!board_id_data.get())
          continue;
      }

      if (strcmp(kSupportedSensors[i].board_id,
                 (const char*)CFDataGetBytePtr(board_id_data)) != 0) {
        continue;
      }
    }

    // Local hardware found in the supported sensor list.
    sensor_candidate = &kSupportedSensors[i];

    // Get a dictionary of the services matching to the one in the sensor.
    CFMutableDictionaryRef dict =
        IOServiceMatching(kGenericSensor.service_name);
    if (!dict)
      continue;

    // Get the first matching service.
    io_service_t device = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                      dict);
    if (!device)
      continue;

    // Try to open device.
    kern_return_t result;
    result = IOServiceOpen(device, mach_task_self(), 0, &io_connection_);
    IOObjectRelease(device);
    if (result != KERN_SUCCESS || io_connection_ == 0)
      return false;

    // Local sensor service confirmed by IOKit.
    sensor_ = sensor_candidate;
    break;
  }

  if (sensor_ == nullptr)
    return false;

  // Allocate and initialize input/output records.
  input_record_.resize(kGenericSensor.record_size, 0x01);
  output_record_.resize(kGenericSensor.record_size, 0x00);

  // Try to retrieve the current orientation.
  float test_axes[3];
  return ReadSensorValues(test_axes);
}

// Extend the sign of an integer of less than 32 bits to a 32-bit integer.
int SuddenMotionSensor::ExtendSign(int value, size_t size) {
  switch (size) {
  case 1:
    if (value & 0x00000080)
      return value | 0xffffff00;
    break;

  case 2:
    if (value & 0x00008000)
      return value | 0xffff0000;
    break;

  case 3:
    if (value & 0x00800000)
      return value | 0xff000000;
    break;

  default:
    LOG(FATAL) << "Invalid integer size for sign extension: " << size;
  }

  return value;
}

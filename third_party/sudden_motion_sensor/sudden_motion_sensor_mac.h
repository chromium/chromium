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

#ifndef SUDDEN_MOTION_SENSOR_H_
#define SUDDEN_MOTION_SENSOR_H_
#pragma once

#include <vector>

#include <IOKit/IOKitLib.h>

// Provides an easy-to-use interface to retrieve data
// from the MacBook family accelerometer.
class SuddenMotionSensor {
 public:
  static SuddenMotionSensor* Create();
  bool ReadSensorValues(float axes[3]);
  ~SuddenMotionSensor();

 private:
  SuddenMotionSensor();
  bool Init();

  // Extend the sign of an integer of size bytes to a 32-bit one.
  static int ExtendSign(int value, size_t size);

  struct GenericMacbookSensor;
  struct SensorDescriptor;
  struct AxisData;

  // Generic sensor data and list of supported sensors.
  static const GenericMacbookSensor kGenericSensor;
  static const SensorDescriptor kSupportedSensors[];

  // Local sensor if supported, else NULL.
  const SensorDescriptor* sensor_;

  // IOKit connection to the local sensor.
  io_connect_t io_connection_;

  // Input memory used for sensor I/O.
  std::vector<char> input_record_;

  // Output memory used for sensor I/O.
  std::vector<char> output_record_;
};

#endif  // SUDDEN_MOTION_SENSOR_H_

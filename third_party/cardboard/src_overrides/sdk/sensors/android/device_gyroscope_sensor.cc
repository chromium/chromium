/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "third_party/cardboard/src/sdk/sensors/device_gyroscope_sensor.h"

#include <android/looper.h>
#include <android/sensor.h>
#include <stddef.h>

#include <memory>

#include "third_party/cardboard/src/sdk/sensors/accelerometer_data.h"
#include "third_party/cardboard/src/sdk/sensors/gyroscope_data.h"
#include "third_party/cardboard/src/sdk/util/constants.h"
#include "third_party/cardboard/src/sdk/util/logging.h"

// Workaround to avoid the inclusion of "android_native_app_glue.h.
#ifndef LOOPER_ID_USER
#define LOOPER_ID_USER 3
#endif

namespace cardboard {

namespace {

// Creates an Android sensor event queue for the current thread.
static ASensorEventQueue* CreateSensorQueue(ASensorManager* sensor_manager) {
  ALooper* event_looper = ALooper_forThread();

  if (event_looper == nullptr) {
    event_looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    CARDBOARD_LOGI(
        "AccelerometerSensor: Created new event looper for gyroscope sensor "
        "capture thread.");
  }

  return ASensorManager_createEventQueue(sensor_manager, event_looper,
                                         LOOPER_ID_USER, nullptr, nullptr);
}

// Initialize Gyroscope sensor on Android. If available we try to
// request a SENSOR_TYPE_GYROSCOPE_UNCALIBRATED.
// Since both seem to be using the same underlying code this will work if the
// same integer is used as the mode as in java.
// The reason for using the uncalibrated gyroscope is that the regular
// gyro is calibrated with a bias offset in the system. As we cannot influence
// the behavior of this algorithm and it will affect the gyro while moving,
// it is safer to initialize to the uncalibrated one and handle the gyro bias
// estimation in Cardboard SDK.
enum PrivateSensors {
  // This is not defined in the native public sensors API, but it is in java.
  // If we define this here and it gets defined later in NDK this should
  // not compile.
  // It is defined in AOSP in hardware/libhardware/include/hardware/sensors.h
  ASENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED = 14,
  ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED = 16,
  ASENSOR_TYPE_ADDITIONAL_INFO = 33,
};

static const ASensor* GetUncalibratedGyroscope(ASensorManager* sensor_manager) {
  return ASensorManager_getDefaultSensor(sensor_manager,
                                         ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED);
}

const ASensor* InitSensor(ASensorManager* sensor_manager) {
  const ASensor* gyro = GetUncalibratedGyroscope(sensor_manager);
  if (gyro != nullptr) {
    CARDBOARD_LOGI("Android Gyro Sensor: ASENSOR_TYPE_GYRO_UNCALIBRATED");
    return gyro;
  }
  CARDBOARD_LOGI("Android Gyro Sensor: ASENSOR_TYPE_GYROSCOPE");
  return ASensorManager_getDefaultSensor(sensor_manager,
                                         ASENSOR_TYPE_GYROSCOPE);
}

bool PollLooper(int timeout_ms, int* num_events) {
  void* source = nullptr;
  const int looper_id = ALooper_pollOnce(timeout_ms, NULL, num_events,
                                        reinterpret_cast<void**>(&source));
  if (looper_id != LOOPER_ID_USER) {
    return false;
  }
  if (*num_events <= 0) {
    return false;
  }
  return true;
}

class SensorEventQueueReader {
 public:
  SensorEventQueueReader(ASensorManager* manager, const ASensor* sensor)
      : manager_(manager),
        sensor_(sensor),
        queue_(CreateSensorQueue(manager_)) {}

  ~SensorEventQueueReader() {
    ASensorManager_destroyEventQueue(manager_, queue_);
  }

  bool Start() {
    ASensorEventQueue_enableSensor(queue_, sensor_);
    const int min_delay = ASensor_getMinDelay(sensor_);
    // Set sensor capture rate to the highest possible sampling rate.
    ASensorEventQueue_setEventRate(queue_, sensor_, min_delay);
    return true;
  }

  void Stop() { ASensorEventQueue_disableSensor(queue_, sensor_); }

  bool WaitForEvent(int timeout_ms, ASensorEvent* event) {
    int num_events;
    if (!PollLooper(timeout_ms, &num_events)) {
      return false;
    }
    return (ASensorEventQueue_getEvents(queue_, event, 1) > 0);
  }

  bool ReadEvent(ASensorEvent* event) {
    return (ASensorEventQueue_getEvents(queue_, event, 1) > 0);
  }

 private:
  ASensorManager* manager_;   // Owned by android library.
  const ASensor* sensor_;     // Owned by android library.
  ASensorEventQueue* queue_;  // Owned by this.
};

}  // namespace

// This struct holds android gyroscope specific sensor information.
struct DeviceGyroscopeSensor::SensorInfo {
  SensorInfo() : sensor_manager(nullptr), sensor(nullptr) {}
  ASensorManager* sensor_manager;
  const ASensor* sensor;
  std::unique_ptr<SensorEventQueueReader> reader;
};

namespace {

bool ParseGyroEvent(const ASensorEvent& event, GyroscopeData* sample) {
  if (event.type == ASENSOR_TYPE_ADDITIONAL_INFO) {
    CARDBOARD_LOGI("ParseGyroEvent discarding additional info sensor event");
    return false;
  }

  sample->sensor_timestamp_ns = event.timestamp;
  sample->system_timestamp = event.timestamp;  // Clock::time_point();
  // The event values in ASensorEvent (event, acceleration and
  // magnetic) are all in the same union type so they can be
  // accessed by event.
  if (event.type == ASENSOR_TYPE_GYROSCOPE) {
    sample->data = {event.vector.x, event.vector.y, event.vector.z};
    return true;
  } else if (event.type == ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED) {
    // This is a special case when it is possible to initialize to
    // ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED
    sample->data = {event.vector.x, event.vector.y, event.vector.z};
    return true;
  } else {
    CARDBOARD_LOGE("ParseGyroEvent discarding unexpected sensor event type %d",
                   event.type);
  }

  return false;
}

}  // namespace

DeviceGyroscopeSensor::DeviceGyroscopeSensor()
    : sensor_info_(new SensorInfo()) {
#if __ANDROID_MIN_SDK_VERSION__ >= 26
  sensor_info_->sensor_manager =
      ASensorManager_getInstanceForPackage(Constants::kCardboardSdkPackageName);
#else
  // TODO: b/314792983 - Remove deprecated NDK methods.
  sensor_info_->sensor_manager = ASensorManager_getInstance();
#endif
  sensor_info_->sensor = InitSensor(sensor_info_->sensor_manager);
  if (!sensor_info_->sensor) {
    return;
  }

  sensor_info_->reader =
      std::unique_ptr<SensorEventQueueReader>(new SensorEventQueueReader(
          sensor_info_->sensor_manager, sensor_info_->sensor));
}

DeviceGyroscopeSensor::~DeviceGyroscopeSensor() {}

void DeviceGyroscopeSensor::PollForSensorData(
    int timeout_ms, std::vector<GyroscopeData>* results) const {
  results->clear();
  ASensorEvent event;
  if (!sensor_info_->reader->WaitForEvent(timeout_ms, &event)) {
    return;
  }
  do {
    GyroscopeData sample;
    if (ParseGyroEvent(event, &sample)) {
      results->push_back(sample);
    }
  } while (sensor_info_->reader->ReadEvent(&event));
}

bool DeviceGyroscopeSensor::Start() {
  if (!sensor_info_->reader) {
    CARDBOARD_LOGE("Could not start gyroscope sensor.");
    return false;
  }
  return sensor_info_->reader->Start();
}

void DeviceGyroscopeSensor::Stop() {
  if (!sensor_info_->reader) {
    return;
  }
  sensor_info_->reader->Stop();
}

}  // namespace cardboard

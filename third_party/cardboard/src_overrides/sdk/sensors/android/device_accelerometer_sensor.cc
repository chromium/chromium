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
#include "third_party/cardboard/src/sdk/sensors/device_accelerometer_sensor.h"

#include <android/looper.h>
#include <android/sensor.h>
#include <stddef.h>

#include <memory>
#include <mutex>  // NOLINT

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
        "AccelerometerSensor: Created new event looper for accelerometer "
        "sensor capture thread.");
  }

  return ASensorManager_createEventQueue(sensor_manager, event_looper,
                                         LOOPER_ID_USER, nullptr, nullptr);
}

const ASensor* InitSensor(ASensorManager* sensor_manager) {
  return ASensorManager_getDefaultSensor(sensor_manager,
                                         ASENSOR_TYPE_ACCELEROMETER);
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

void ParseAccelerometerEvent(const ASensorEvent& event,
                             AccelerometerData* sample) {
  sample->sensor_timestamp_ns = event.timestamp;
  sample->system_timestamp = event.timestamp;
  // The event values in ASensorEvent (event, acceleration and
  // magnetic) are all in the same union type so they can be
  // accessed by event.
  sample->data = {event.vector.x, event.vector.y, event.vector.z};
}

}  // namespace

// This struct holds android specific sensor information.
struct DeviceAccelerometerSensor::SensorInfo {
  SensorInfo() : sensor_manager(nullptr), sensor(nullptr) {}

  ASensorManager* sensor_manager;
  const ASensor* sensor;
  std::unique_ptr<SensorEventQueueReader> reader;
};

DeviceAccelerometerSensor::DeviceAccelerometerSensor()
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

DeviceAccelerometerSensor::~DeviceAccelerometerSensor() {}

void DeviceAccelerometerSensor::PollForSensorData(
    int timeout_ms, std::vector<AccelerometerData>* results) const {
  results->clear();
  ASensorEvent event;
  if (!sensor_info_->reader->WaitForEvent(timeout_ms, &event)) {
    return;
  }
  do {
    AccelerometerData sample;
    ParseAccelerometerEvent(event, &sample);
    results->push_back(sample);
  } while (sensor_info_->reader->ReadEvent(&event));
}

bool DeviceAccelerometerSensor::Start() {
  if (!sensor_info_->reader) {
    CARDBOARD_LOGE("Could not start accelerometer sensor");
    return false;
  }
  return sensor_info_->reader->Start();
}

void DeviceAccelerometerSensor::Stop() {
  if (!sensor_info_->reader) {
    return;
  }
  sensor_info_->reader->Stop();
}

}  // namespace cardboard

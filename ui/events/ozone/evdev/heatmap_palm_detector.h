// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_HEATMAP_PALM_DETECTOR_H_
#define UI_EVENTS_OZONE_EVDEV_HEATMAP_PALM_DETECTOR_H_

#include <memory>

#include <string_view>

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace ui {

// Interface for touch screen heatmap palm detector.
class COMPONENT_EXPORT(EVDEV) HeatmapPalmDetector {
 public:
  enum class DetectionResult { kNoPalm = 0, kPalm = 1 };
  enum class DeviceId {
    kRex = 0,
    kGeralt = 1,
  };

  using DetectionDoneCallback = base::OnceCallback<void(DetectionResult)>;

  static void SetInstance(std::unique_ptr<HeatmapPalmDetector> detector);

  static HeatmapPalmDetector* GetInstance();

  HeatmapPalmDetector();
  HeatmapPalmDetector(const HeatmapPalmDetector&) = delete;
  HeatmapPalmDetector& operator=(const HeatmapPalmDetector&) = delete;
  virtual ~HeatmapPalmDetector();

  // Starts the palm detection service based on the device id and path.
  virtual void Start(DeviceId device, std::string_view path) = 0;

  // Gets the palm detection results of the latest heatmap data.
  virtual DetectionResult GetDetectionResult() const = 0;

  // Returns if the palm detection results is ready.
  virtual bool IsReady() const = 0;
};

}  // namespace ui
#endif  // UI_EVENTS_OZONE_EVDEV_HEATMAP_PALM_DETECTOR_H_

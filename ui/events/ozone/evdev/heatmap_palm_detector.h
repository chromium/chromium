// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_HEATMAP_PALM_DETECTOR_H_
#define UI_EVENTS_OZONE_EVDEV_HEATMAP_PALM_DETECTOR_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace ui {

// Interface for touch screen heatmap palm detector.
class COMPONENT_EXPORT(EVDEV) HeatmapPalmDetector {
 public:
  enum class ModelId {
    kNotSupported = 0,
    kRex = 1,
    kGeralt = 2,
  };
  struct TouchRecord {
    TouchRecord(base::Time timestamp, const std::vector<int>& tracking_ids);
    TouchRecord(const TouchRecord& t);
    ~TouchRecord();

    base::Time timestamp;
    std::vector<int> tracking_ids;
  };

  static void SetInstance(std::unique_ptr<HeatmapPalmDetector> detector);

  static HeatmapPalmDetector* GetInstance();

  HeatmapPalmDetector();
  HeatmapPalmDetector(const HeatmapPalmDetector&) = delete;
  HeatmapPalmDetector& operator=(const HeatmapPalmDetector&) = delete;
  virtual ~HeatmapPalmDetector();

  // Starts the palm detection service based on the model id and hidraw path.
  virtual void Start(ModelId model_id, std::string_view hidraw_path) = 0;

  // Returns if a touch, specified with `tracking_id`, is detected as palm.
  virtual bool IsPalm(int tracking_id) const = 0;

  // Returns if the palm detection results is ready.
  virtual bool IsReady() const = 0;

  // Adds a touch record containing timestamp and traicking ids, we will use the
  // timestamp to match touch record with heatmap palm detection results.
  virtual void AddTouchRecord(base::Time timestamp,
                              const std::vector<int>& tracking_ids) = 0;

  // Removes the tracking id which is no longer on the screen.
  virtual void RemoveTouch(int tracking_id) = 0;
};

}  // namespace ui
#endif  // UI_EVENTS_OZONE_EVDEV_HEATMAP_PALM_DETECTOR_H_

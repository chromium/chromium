// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PALM_DETECTOR_H_
#define UI_OZONE_PUBLIC_PALM_DETECTOR_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {

// Interface for touch screen palm detector.
class COMPONENT_EXPORT(OZONE_BASE) PalmDetector {
 public:
  enum class DetectionResult { kNoPalm = 0, kPalm = 1 };
  enum class DeviceId {
    kRex = 0,
    kGeralt = 1,
  };

  using DetectionDoneCallback = base::OnceCallback<void(DetectionResult)>;

  PalmDetector();
  PalmDetector(const PalmDetector&) = delete;
  PalmDetector& operator=(const PalmDetector&) = delete;
  virtual ~PalmDetector();

  // Starts the palm detection service based on the device id and path.
  virtual void Start(DeviceId device, std::string_view path) = 0;

  // Gets the palm detection results of the latest heatmap data.
  virtual DetectionResult GetDetectionResult() const = 0;

  // Returns if the palm detection results is ready.
  bool IsReady() const { return is_ready_; }

 protected:
  bool is_ready_ = false;
};

}  // namespace ui
#endif  // UI_OZONE_PUBLIC_PALM_DETECTOR_H_

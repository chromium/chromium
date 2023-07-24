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

  using DetectionDoneCallback = base::OnceCallback<void(DetectionResult)>;

  PalmDetector();
  PalmDetector(const PalmDetector&) = delete;
  PalmDetector& operator=(const PalmDetector&) = delete;
  virtual ~PalmDetector();

  // Detects if a frame of heatmap data, provided by `data`, contains a palm.
  // The `callback` will be supplied with the detection result asynchronously.
  virtual void DetectPalm(const std::vector<double>& data,
                          DetectionDoneCallback callback) = 0;
};

}  // namespace ui
#endif  // UI_OZONE_PUBLIC_PALM_DETECTOR_H_

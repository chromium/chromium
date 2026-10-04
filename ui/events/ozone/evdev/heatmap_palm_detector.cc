// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/heatmap_palm_detector.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ui {

namespace {
HeatmapPalmDetector* g_instance = nullptr;
}  // namespace

HeatmapPalmDetector::HeatmapPalmDetector() {
  CHECK(!g_instance);
  g_instance = this;
}

HeatmapPalmDetector::~HeatmapPalmDetector() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

HeatmapPalmDetector::TouchRecord::TouchRecord(
    base::Time timestamp,
    const std::vector<int>& tracking_ids) {
  this->timestamp = timestamp;
  this->tracking_ids = tracking_ids;
}

HeatmapPalmDetector::TouchRecord::TouchRecord(
    const HeatmapPalmDetector::TouchRecord& t) = default;

HeatmapPalmDetector::TouchRecord::~TouchRecord() = default;

// static
HeatmapPalmDetector* HeatmapPalmDetector::GetInstance() {
  return g_instance;
}

}  // namespace ui

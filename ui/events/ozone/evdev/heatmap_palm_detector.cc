// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/heatmap_palm_detector.h"

namespace ui {

namespace {
std::unique_ptr<HeatmapPalmDetector> g_instance = nullptr;
}  // namespace

HeatmapPalmDetector::HeatmapPalmDetector() = default;

HeatmapPalmDetector::~HeatmapPalmDetector() = default;

void HeatmapPalmDetector::SetInstance(
    std::unique_ptr<HeatmapPalmDetector> detector) {
  g_instance = std::move(detector);
}

HeatmapPalmDetector* HeatmapPalmDetector::GetInstance() {
  return g_instance.get();
}

}  // namespace ui

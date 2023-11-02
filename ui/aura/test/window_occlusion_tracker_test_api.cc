// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/window_occlusion_tracker_test_api.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace aura {
namespace test {

WindowOcclusionTrackerTestApi::WindowOcclusionTrackerTestApi(
    WindowOcclusionTracker* tracker)
    : tracker_(tracker) {}

WindowOcclusionTrackerTestApi::~WindowOcclusionTrackerTestApi() = default;

// static
std::unique_ptr<WindowOcclusionTracker>
WindowOcclusionTrackerTestApi::Create() {
  // Use base::WrapUnique + new because of the constructor is private.
  return base::WrapUnique(new WindowOcclusionTracker());
}

int WindowOcclusionTrackerTestApi::GetNumTimesOcclusionRecomputed() const {
  return tracker_->num_times_occlusion_recomputed_;
}

void WindowOcclusionTrackerTestApi::Pause() {
  tracker_->Pause();
}

void WindowOcclusionTrackerTestApi::Unpause() {
  tracker_->Unpause();
}

bool WindowOcclusionTrackerTestApi::IsPaused() const {
  return tracker_->num_pause_occlusion_tracking_;
}

}  // namespace test
}  // namespace aura

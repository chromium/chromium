// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/drm_render_node_path_finder.h"

#include <vector>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(DrmRenderNodePathFinderTest, EmptyCandidatesReturnsEmptyPath) {
  EXPECT_TRUE(
      DrmRenderNodePathFinder::SelectRenderNodeFromCandidates({}).empty());
}

TEST(DrmRenderNodePathFinderTest, SelectsPreferredDriverOverNonPreferred) {
  const base::FilePath fallback_path("/dev/dri/renderD128");
  const base::FilePath preferred_path("/dev/dri/renderD129");

  // Verify that a preferred driver (e.g. i915) is selected even when listed
  // after a non-preferred driver.
  std::vector<DrmRenderNodeCandidate> candidates = {
      {"nouveau", fallback_path},
      {"i915", preferred_path},
  };
  EXPECT_EQ(DrmRenderNodePathFinder::SelectRenderNodeFromCandidates(candidates),
            preferred_path);
}

TEST(DrmRenderNodePathFinderTest, RespectsVendorPriorityOrder) {
  const base::FilePath i915_path("/dev/dri/renderD128");
  const base::FilePath amdgpu_path("/dev/dri/renderD129");

  // Verify the priority order contract: i915 takes precedence over amdgpu.
  std::vector<DrmRenderNodeCandidate> candidates = {
      {"amdgpu", amdgpu_path},
      {"i915", i915_path},
  };
  EXPECT_EQ(DrmRenderNodePathFinder::SelectRenderNodeFromCandidates(candidates),
            i915_path);
}

TEST(DrmRenderNodePathFinderTest, FallsBackToFirstCandidateWhenNonePreferred) {
  const base::FilePath first_path("/dev/dri/renderD128");
  const base::FilePath second_path("/dev/dri/renderD129");

  // Verify fallback contract: when no drivers match the preference list,
  // the first candidate is chosen.
  std::vector<DrmRenderNodeCandidate> candidates = {
      {"msm", first_path},
      {"nouveau", second_path},
  };
  EXPECT_EQ(DrmRenderNodePathFinder::SelectRenderNodeFromCandidates(candidates),
            first_path);
}

}  // namespace ui
